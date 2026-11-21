#include "../include/brainco_hands.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, char **argv)
{
    if (argc != 3 || (std::string(argv[2]) != "--move" &&
                      std::string(argv[2]) != "--touch-only"))
    {
        std::cerr << "Usage: " << argv[0]
                  << " <network-interface> [--touch-only|--move]\n";
        std::cerr << "This performs a real low-speed 0.05 index-finger motion on both hands.\n";
        return 2;
    }

    BraincoHands hands;
    hands.initialize(argv[1]);
    hands.SetMaxCloseRatio(0.85);
    hands.SetMaxSpeed(0.1);

    if (std::string(argv[2]) == "--touch-only")
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while ((!hands.hasTouch(true) || !hands.hasTouch(false)) &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!hands.hasTouch(true) || !hands.hasTouch(false))
        {
            std::cerr << "Timed out waiting for both BrainCo touch topics\n";
            return 3;
        }
        const auto left_touch = hands.getTouch(true);
        const auto right_touch = hands.getTouch(false);
        std::cout << "left_status=";
        for (int i = 0; i < 5; ++i)
            std::cout << (i ? "," : "") << left_touch->states()[i].lost();
        std::cout << " right_status=";
        for (int i = 0; i < 5; ++i)
            std::cout << (i ? "," : "") << right_touch->states()[i].lost();
        std::cout << "\nBRAINCO_TOUCH_DDS_CLIENT_PASS\n";
        return 0;
    }

    const auto state_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((!hands.hasState(true) || !hands.hasState(false)) &&
           std::chrono::steady_clock::now() < state_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!hands.hasState(true) || !hands.hasState(false))
    {
        std::cerr << "Timed out waiting for both BrainCo state topics\n";
        return 3;
    }

    std::array<double, 7> left{};
    std::array<double, 7> right{};
    const auto left_state = hands.getState(true);
    const auto right_state = hands.getState(false);
    for (int i = 0; i < BRAINCO_MOTOR_MAX; ++i)
    {
        left[i] = left_state->states()[i].q();
        right[i] = right_state->states()[i].q();
    }
    const auto left_home = left;
    const auto right_home = right;
    left[2] = std::min(left[2] + 0.05, 0.85);
    right[2] = std::min(right[2] + 0.05, 0.85);

    hands.setAllJointsCommand(true, left);
    hands.setAllJointsCommand(false, right);
    for (int i = 0; i < 150; ++i)
    {
        hands.writeOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    hands.setAllJointsCommand(true, left_home);
    hands.setAllJointsCommand(false, right_home);
    for (int i = 0; i < 150; ++i)
    {
        hands.writeOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto left_final = hands.getState(true);
    const auto right_final = hands.getState(false);
    std::cout << "left index final=" << left_final->states()[2].q()
              << " right index final=" << right_final->states()[2].q() << '\n';
    std::cout << "BRAINCO_DDS_SMOKE_PASS\n";
    return 0;
}
