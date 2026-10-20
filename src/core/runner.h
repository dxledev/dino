#pragma once

#include "sprites.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>

namespace dino {

enum class GameState {
    Ready,
    Running,
    GameOver,
};

enum class ObstacleType {
    Cactus,
    Bird,
};

struct Rect {
    double x{};
    double y{};
    double width{};
    double height{};
};

struct Input {
    bool jumpPressed{};
    bool jumpHeld{};
    bool duckHeld{};
};

struct Obstacle {
    ObstacleType type{ObstacleType::Cactus};
    Rect bounds{};
    double speedOffset{};
};

struct Config {
    double fixedStepSeconds{1.0 / 120.0};
    double maxFrameDeltaSeconds{0.25};
    double worldWidth{600.0};
    double groundY{140.0};
    double playerX{50.0};
    double playerWidth{42.0};
    double standingHeight{40.0};
    double duckHeight{22.0};
    double duckWidth{60.0};
    double clearSeconds{3.0};
    double scorePerUnit{0.025};
    double birdMinimumSpeed{510.0};
    double initialSpeed{360.0};
    double maxSpeed{780.0};
    double acceleration{3.6};
    double jumpVelocity{-636.0};
    double gravity{2160.0};
    double heldJumpGravityScale{1.0};
    double maxJumpHoldSeconds{0.18};
    double fastFallGravityScale{2.3};
    double maxFallSpeed{1150.0};
    double minimumObstacleGap{120.0};
    double minimumGapSeconds{0.6};
    double maximumGapSeconds{0.9};
};

class RunnerEngine {
public:
    static constexpr std::size_t MaxObstacles = 16;

    explicit RunnerEngine(std::uint32_t seed = 0xD1A05EEDu, Config config = {});

    void setWorldWidth(double width) noexcept;
    void reset(std::uint32_t seed = 0xD1A05EEDu);
    void start() noexcept;
    void update(double frameDeltaSeconds, Input input = {}) noexcept;

    [[nodiscard]] GameState state() const noexcept;
    [[nodiscard]] std::uint64_t score() const noexcept;
    [[nodiscard]] SpriteId playerSprite() const noexcept;
    [[nodiscard]] SpriteId obstacleSprite(const Obstacle &obstacle) const noexcept;
    [[nodiscard]] double distanceTravelled() const noexcept;
    [[nodiscard]] double night() const noexcept;
    [[nodiscard]] bool scoreVisible() const noexcept;
    [[nodiscard]] double speed() const noexcept;
    [[nodiscard]] double elapsedSeconds() const noexcept;
    [[nodiscard]] Rect playerBounds() const noexcept;
    [[nodiscard]] double playerVelocityY() const noexcept;
    [[nodiscard]] bool playerGrounded() const noexcept;
    [[nodiscard]] std::span<const Obstacle> obstacles() const noexcept;
    [[nodiscard]] const Config& config() const noexcept;

private:
    void fixedUpdate(double deltaSeconds) noexcept;
    void updatePlayer(double deltaSeconds) noexcept;
    void updateObstacles(double deltaSeconds) noexcept;
    [[nodiscard]] double spawnObstacleGroup() noexcept;
    void compactObstacles() noexcept;
    void checkCollision() noexcept;
    void addObstacle(ObstacleType type, double x, double width, double height,
                     double y, double speedOffset = 0.0) noexcept;
    [[nodiscard]] double obstacleGap() noexcept;
    void updateEffects(double deltaSeconds, std::uint64_t previousScore) noexcept;

    Config config_;
    std::mt19937 random_;
    GameState state_{GameState::Ready};
    std::array<Obstacle, MaxObstacles> obstacles_{};
    std::size_t obstacleCount_{};
    double accumulator_{};
    double elapsedSeconds_{};
    double distanceTravelled_{};
    double night_{};
    double nightRemaining_{};
    double flashRemaining_{};
    double distanceUntilSpawn_{};
    double speed_{};
    double playerY_{};
    double playerVelocityY_{};
    double jumpHoldRemaining_{};
    bool playerGrounded_{true};
    bool jumpQueued_{};
    bool jumpHeld_{};
    bool duckHeld_{};
    std::uint64_t score_{};
};

} // namespace dino
