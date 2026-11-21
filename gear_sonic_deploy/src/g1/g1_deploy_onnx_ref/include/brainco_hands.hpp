/**
 * @file brainco_hands.hpp
 * @brief DDS client for a pair of BrainCo Revo2 hands.
 *
 * The robot-side brainco_hand_service converts these DDS commands to the two
 * RS485 links. Revo2 exposes six active joints per hand in normalized [0, 1]
 * units: thumb flex, thumb auxiliary, index, middle, ring, and pinky.
 */

#ifndef BRAINCO_HANDS_HPP
#define BRAINCO_HANDS_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include <unitree/idl/go2/MotorCmds_.hpp>
#include <unitree/idl/go2/MotorStates_.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_publisher.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>

#include "utils.hpp"

static constexpr int BRAINCO_MOTOR_MAX = 6;

class BraincoHands
{
public:
    BraincoHands() = default;

    void initialize(const std::string &network_interface)
    {
        if (!network_interface.empty())
        {
            unitree::robot::ChannelFactory::Instance()->Init(0, network_interface.c_str());
        }

        initializeHand(left_, "left");
        initializeHand(right_, "right");
        initialized_ = true;
        std::cout << "[BraincoHands] DDS channels initialized (6 DOF/hand)" << std::endl;
    }

    void SetMaxCloseRatio(double ratio)
    {
        max_close_ratio_.store(std::clamp(ratio, 0.2, 1.0));
    }

    double GetMaxCloseRatio() const { return max_close_ratio_.load(); }

    void SetMaxSpeed(double speed)
    {
        max_speed_.store(std::clamp(speed, 0.01, 1.0));
    }

    void setAllJointsCommand(bool is_left, const std::array<double, 7> &q)
    {
        HandCtx &ctx = is_left ? left_ : right_;
        unitree_go::msg::dds_::MotorCmds_ cmd;
        cmd.cmds().resize(BRAINCO_MOTOR_MAX);
        const double close_limit = max_close_ratio_.load();
        const double speed_limit = max_speed_.load();
        for (int i = 0; i < BRAINCO_MOTOR_MAX; ++i)
        {
            cmd.cmds()[i].q(static_cast<float>(std::clamp(q[i], 0.0, close_limit)));
            cmd.cmds()[i].dq(static_cast<float>(speed_limit));
        }
        ctx.cmd_buffer.SetData(std::move(cmd));
        ctx.has_command.store(true);
    }

    void writeOnce()
    {
        if (!initialized_) return;

        const auto now = std::chrono::steady_clock::now();
        if (now - last_publish_ < kPublishPeriod) return;
        last_publish_ = now;

        publishHand(left_);
        publishHand(right_);
    }

    std::shared_ptr<const unitree_go::msg::dds_::MotorStates_> getState(bool is_left) const
    {
        const HandCtx &ctx = is_left ? left_ : right_;
        return ctx.state_buffer.GetDataWithTime().data;
    }

    bool hasState(bool is_left) const
    {
        const HandCtx &ctx = is_left ? left_ : right_;
        return ctx.state_buffer.GetDataWithTime().HasData();
    }

    std::shared_ptr<const unitree_go::msg::dds_::MotorStates_> getTouch(bool is_left) const
    {
        const HandCtx &ctx = is_left ? left_ : right_;
        return ctx.touch_buffer.GetDataWithTime().data;
    }

    bool hasTouch(bool is_left) const
    {
        const HandCtx &ctx = is_left ? left_ : right_;
        return ctx.touch_buffer.GetDataWithTime().HasData();
    }

private:
    struct HandCtx
    {
        std::unique_ptr<unitree::robot::ChannelPublisher<unitree_go::msg::dds_::MotorCmds_>> publisher;
        std::unique_ptr<unitree::robot::ChannelSubscriber<unitree_go::msg::dds_::MotorStates_>> subscriber;
        std::unique_ptr<unitree::robot::ChannelSubscriber<unitree_go::msg::dds_::MotorStates_>> touch_subscriber;
        DataBuffer<unitree_go::msg::dds_::MotorCmds_> cmd_buffer;
        DataBuffer<unitree_go::msg::dds_::MotorStates_> state_buffer;
        DataBuffer<unitree_go::msg::dds_::MotorStates_> touch_buffer;
        std::atomic<bool> has_command{false};
    };

    void initializeHand(HandCtx &ctx, const std::string &side)
    {
        const std::string topic_base = "rt/brainco/" + side;
        ctx.publisher = std::make_unique<unitree::robot::ChannelPublisher<unitree_go::msg::dds_::MotorCmds_>>(
            topic_base + "/cmd");
        ctx.subscriber = std::make_unique<unitree::robot::ChannelSubscriber<unitree_go::msg::dds_::MotorStates_>>(
            topic_base + "/state");
        ctx.touch_subscriber = std::make_unique<unitree::robot::ChannelSubscriber<unitree_go::msg::dds_::MotorStates_>>(
            topic_base + "/touch");
        ctx.publisher->InitChannel();
        HandCtx *ctx_ptr = &ctx;
        ctx.subscriber->InitChannel(
            [ctx_ptr](const void *message) {
                const auto &state = *static_cast<const unitree_go::msg::dds_::MotorStates_ *>(message);
                if (state.states().size() == BRAINCO_MOTOR_MAX)
                {
                    ctx_ptr->state_buffer.SetData(state);
                }
            },
            1);
        ctx.touch_subscriber->InitChannel(
            [ctx_ptr](const void *message) {
                const auto &touch = *static_cast<const unitree_go::msg::dds_::MotorStates_ *>(message);
                if (touch.states().size() == 5)
                {
                    ctx_ptr->touch_buffer.SetData(touch);
                }
            },
            1);
    }

    void publishHand(HandCtx &ctx)
    {
        // Do not publish a synthetic zero command before a real upstream hand
        // target has arrived. This is the first safety barrier at startup.
        if (!ctx.publisher || !ctx.has_command.load()) return;

        const auto cmd_ptr = ctx.cmd_buffer.GetDataWithTime().data;
        if (!cmd_ptr || cmd_ptr->cmds().size() != BRAINCO_MOTOR_MAX) return;

        auto command = *cmd_ptr;
        const auto state_ptr = ctx.state_buffer.GetDataWithTime().data;
        const double close_limit = max_close_ratio_.load();
        const double speed_limit = max_speed_.load();

        for (int i = 0; i < BRAINCO_MOTOR_MAX; ++i)
        {
            double target = std::clamp<double>(command.cmds()[i].q(), 0.0, close_limit);
            if (state_ptr && state_ptr->states().size() == BRAINCO_MOTOR_MAX)
            {
                const double current = state_ptr->states()[i].q();
                target = current + std::clamp(target - current, -kMaxDeltaPerPublish, kMaxDeltaPerPublish);
            }
            command.cmds()[i].q(static_cast<float>(target));
            command.cmds()[i].dq(static_cast<float>(speed_limit));
        }
        ctx.publisher->Write(command);
    }

    static constexpr std::chrono::milliseconds kPublishPeriod{10}; // 100 Hz
    static constexpr double kMaxDeltaPerPublish = 0.02;

    HandCtx left_;
    HandCtx right_;
    std::atomic<double> max_close_ratio_{0.85};
    std::atomic<double> max_speed_{0.2};
    bool initialized_{false};
    std::chrono::steady_clock::time_point last_publish_{};
};

#endif // BRAINCO_HANDS_HPP
