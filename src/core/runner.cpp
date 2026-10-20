#include "runner.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dino {
namespace {

constexpr std::size_t MaxFixedUpdatesPerFrame = 64;
constexpr std::size_t MaxSpawnGroupsPerUpdate = 16;

bool isFinitePositive(double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

double randomUnit(std::mt19937& random) noexcept
{
    return static_cast<double>(random())
        / (static_cast<double>(std::mt19937::max()) + 1.0);
}

} // namespace

RunnerEngine::RunnerEngine(std::uint32_t seed, Config config)
    : config_(config)
    , random_(seed)
{
    if (!isFinitePositive(config_.fixedStepSeconds)) {
        config_.fixedStepSeconds = 1.0 / 120.0;
    }
    if (!isFinitePositive(config_.maxFrameDeltaSeconds)) {
        config_.maxFrameDeltaSeconds = 0.25;
    }
    if (!isFinitePositive(config_.worldWidth)) {
        config_.worldWidth = 600.0;
    }
    if (!isFinitePositive(config_.standingHeight)) {
        config_.standingHeight = 40.0;
    }
    if (!isFinitePositive(config_.duckHeight) || config_.duckHeight > config_.standingHeight) {
        config_.duckHeight = config_.standingHeight * 0.6;
    }
    if (!isFinitePositive(config_.playerWidth)) {
        config_.playerWidth = 42.0;
    }
    if (!std::isfinite(config_.groundY)) {
        config_.groundY = 140.0;
    }
    if (!std::isfinite(config_.playerX)) {
        config_.playerX = 50.0;
    }
    if (!isFinitePositive(config_.initialSpeed)) {
        config_.initialSpeed = 360.0;
    }
    if (!isFinitePositive(config_.maxSpeed) || config_.maxSpeed < config_.initialSpeed) {
        config_.maxSpeed = config_.initialSpeed;
    }
    if (!isFinitePositive(config_.acceleration)) {
        config_.acceleration = 3.6;
    }
    if (!isFinitePositive(config_.gravity)) {
        config_.gravity = 2160.0;
    }
    if (!isFinitePositive(config_.maxFallSpeed)) {
        config_.maxFallSpeed = 1150.0;
    }
    if (!isFinitePositive(config_.minimumObstacleGap)) {
        config_.minimumObstacleGap = 120.0;
    }
    if (!isFinitePositive(config_.minimumGapSeconds)) {
        config_.minimumGapSeconds = 0.6;
    }
    if (!std::isfinite(config_.maximumGapSeconds)
        || config_.maximumGapSeconds < config_.minimumGapSeconds) {
        config_.maximumGapSeconds = config_.minimumGapSeconds;
    }
    if (!isFinitePositive(config_.maxJumpHoldSeconds)) {
        config_.maxJumpHoldSeconds = 0.18;
    }
    if (!isFinitePositive(config_.heldJumpGravityScale)) {
        config_.heldJumpGravityScale = 1.0;
    }
    if (!isFinitePositive(config_.fastFallGravityScale)) {
        config_.fastFallGravityScale = 2.3;
    }
    if (!std::isfinite(config_.jumpVelocity) || config_.jumpVelocity >= 0.0) {
        config_.jumpVelocity = -636.0;
    }

    if (!isFinitePositive(config_.duckWidth)) config_.duckWidth = 60.0;
    if (!isFinitePositive(config_.clearSeconds)) config_.clearSeconds = 3.0;
    if (!isFinitePositive(config_.scorePerUnit)) config_.scorePerUnit = 0.025;
    if (!isFinitePositive(config_.birdMinimumSpeed)) config_.birdMinimumSpeed = 510.0;

    reset(seed);
}

void RunnerEngine::setWorldWidth(double width) noexcept
{
    if (isFinitePositive(width)) config_.worldWidth = width;
}

void RunnerEngine::reset(std::uint32_t seed)
{
    random_.seed(seed);
    state_ = GameState::Ready;
    obstacleCount_ = 0;
    accumulator_ = 0.0;
    elapsedSeconds_ = 0.0;
    distanceTravelled_ = 0.0;
    distanceUntilSpawn_ = 0.0;
    night_ = nightRemaining_ = flashRemaining_ = 0.0;
    speed_ = config_.initialSpeed;
    playerY_ = config_.groundY - config_.standingHeight;
    playerVelocityY_ = 0.0;
    jumpHoldRemaining_ = 0.0;
    playerGrounded_ = true;
    jumpQueued_ = false;
    jumpHeld_ = false;
    duckHeld_ = false;
    score_ = 0;
}

void RunnerEngine::start() noexcept
{
    if (state_ == GameState::Ready) {
        state_ = GameState::Running;
    }
}

void RunnerEngine::update(double frameDeltaSeconds, Input input) noexcept
{
    if (state_ != GameState::Running) {
        return;
    }

    jumpQueued_ = jumpQueued_ || input.jumpPressed;
    jumpHeld_ = input.jumpHeld;
    duckHeld_ = input.duckHeld;
    if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0) {
        return;
    }

    accumulator_ += std::min(frameDeltaSeconds, config_.maxFrameDeltaSeconds);
    std::size_t fixedUpdates = 0;
    while (accumulator_ >= config_.fixedStepSeconds
           && state_ == GameState::Running
           && fixedUpdates < MaxFixedUpdatesPerFrame) {
        fixedUpdate(config_.fixedStepSeconds);
        ++fixedUpdates;
        if (state_ != GameState::Running) {
            accumulator_ = 0.0;
            break;
        }
        accumulator_ -= config_.fixedStepSeconds;
    }
    if (fixedUpdates == MaxFixedUpdatesPerFrame) {
        accumulator_ = std::fmod(accumulator_, config_.fixedStepSeconds);
    }
}

GameState RunnerEngine::state() const noexcept
{
    return state_;
}

std::uint64_t RunnerEngine::score() const noexcept
{
    return score_;
}

double RunnerEngine::speed() const noexcept
{
    return speed_;
}

double RunnerEngine::elapsedSeconds() const noexcept
{
    return elapsedSeconds_;
}

Rect RunnerEngine::playerBounds() const noexcept
{
    const double height = playerGrounded_ && duckHeld_ && state_ != GameState::GameOver
        ? config_.duckHeight
        : config_.standingHeight;
    return {config_.playerX, playerY_ + config_.standingHeight - height,
            playerGrounded_ && duckHeld_ && state_ != GameState::GameOver ? config_.duckWidth : config_.playerWidth, height};
}

double RunnerEngine::playerVelocityY() const noexcept
{
    return playerVelocityY_;
}

bool RunnerEngine::playerGrounded() const noexcept
{
    return playerGrounded_;
}

std::span<const Obstacle> RunnerEngine::obstacles() const noexcept
{
    return {obstacles_.data(), obstacleCount_};
}

const Config& RunnerEngine::config() const noexcept
{
    return config_;
}

void RunnerEngine::fixedUpdate(double deltaSeconds) noexcept
{
    if (deltaSeconds > std::numeric_limits<double>::max() - elapsedSeconds_) {
        elapsedSeconds_ = std::numeric_limits<double>::max();
    } else {
        elapsedSeconds_ += deltaSeconds;
    }
    speed_ = std::min(config_.maxSpeed, speed_ + config_.acceleration * deltaSeconds);
    const double distance = speed_ * deltaSeconds;
    if (!std::isfinite(distance) || distance > std::numeric_limits<double>::max() - distanceTravelled_) {
        distanceTravelled_ = std::numeric_limits<double>::max();
    } else {
        distanceTravelled_ += distance;
    }
    const auto previousScore = score_;
    const double rawScore = distanceTravelled_ * config_.scorePerUnit;
    score_ = rawScore >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())
        ? std::numeric_limits<std::uint64_t>::max()
        : static_cast<std::uint64_t>(rawScore);

    updateEffects(deltaSeconds, previousScore);
    updatePlayer(deltaSeconds);
    updateObstacles(deltaSeconds);
    checkCollision();
}

void RunnerEngine::updatePlayer(double deltaSeconds) noexcept
{
    if (jumpQueued_) {
        jumpQueued_ = false;
        if (playerGrounded_ && !duckHeld_) {
            playerGrounded_ = false;
            playerVelocityY_ = config_.jumpVelocity;
            jumpHoldRemaining_ = config_.maxJumpHoldSeconds;
        }
    }

    if (playerGrounded_) {
        playerY_ = config_.groundY - config_.standingHeight;
        playerVelocityY_ = 0.0;
        return;
    }

    const double jumpHeight = config_.groundY - config_.standingHeight - playerY_;
    if ((!jumpHeld_ && jumpHeight > 30.0) || playerY_ < 30.0)
        playerVelocityY_ = std::max(playerVelocityY_, -300.0);
    if (duckHeld_ && playerVelocityY_ < 60.0) playerVelocityY_ = 60.0;
    double gravityScale = 1.0;
    if (duckHeld_) {
        gravityScale *= config_.fastFallGravityScale;
    } else if (jumpHeld_ && playerVelocityY_ < 0.0 && jumpHoldRemaining_ > 0.0) {
        gravityScale *= config_.heldJumpGravityScale;
    }
    if (jumpHoldRemaining_ > 0.0) {
        jumpHoldRemaining_ = std::max(0.0, jumpHoldRemaining_ - deltaSeconds);
    }

    playerVelocityY_ = std::min(config_.maxFallSpeed,
                                playerVelocityY_ + config_.gravity * gravityScale * deltaSeconds);
    playerY_ += playerVelocityY_ * deltaSeconds;
    const double groundTop = config_.groundY - config_.standingHeight;
    if (playerY_ >= groundTop) {
        playerY_ = groundTop;
        playerVelocityY_ = 0.0;
        playerGrounded_ = true;
    }
}

void RunnerEngine::updateObstacles(double deltaSeconds) noexcept
{
    const double rawDistance = speed_ * deltaSeconds;
    const double distance = std::isfinite(rawDistance)
        ? rawDistance
        : std::numeric_limits<double>::max();
    distanceUntilSpawn_ -= distance;
    for (std::size_t index = 0; index < obstacleCount_; ++index) {
        Obstacle& obstacle = obstacles_[index];
        obstacle.bounds.x -= distance;
        if (obstacle.type == ObstacleType::Bird)
            obstacle.bounds.x -= obstacle.speedOffset * deltaSeconds;
    }
    compactObstacles();

    if (elapsedSeconds_ < config_.clearSeconds) { distanceUntilSpawn_ = 0.0; return; }
    std::size_t spawnedGroups = 0;
    while (distanceUntilSpawn_ <= 0.0 && spawnedGroups < MaxSpawnGroupsPerUpdate) {
        const double groupWidth = spawnObstacleGroup();
        distanceUntilSpawn_ += groupWidth + obstacleGap();
        ++spawnedGroups;
    }
    if (!std::isfinite(distanceUntilSpawn_) || distanceUntilSpawn_ <= 0.0) {
        distanceUntilSpawn_ = std::max(config_.minimumObstacleGap,
                                       speed_ * config_.minimumGapSeconds);
        if (!isFinitePositive(distanceUntilSpawn_)) {
            distanceUntilSpawn_ = config_.minimumObstacleGap;
        }
    }
}

double RunnerEngine::spawnObstacleGroup() noexcept
{
    if (speed_ >= config_.birdMinimumSpeed && random_() % 3 == 0) {
        constexpr double heights[]{0.0, 27.0, 52.0};
        const double bottom = config_.groundY - heights[random_() % 3];
        addObstacle(ObstacleType::Bird, config_.worldWidth, 42.0, 20.0,
                    bottom - 20.0, random_() % 2 ? 48.0 : -48.0);
        return 42.0;
    }
    const bool large = random_() % 2;
    const double width = large ? 24.0 : 16.0;
    const double height = large ? 48.0 : 34.0;
    const int count = speed_ > (large ? 420.0 : 240.0) ? static_cast<int>(random_() % 3) + 1 : 1;
    double x = config_.worldWidth;
    for (int index = 0; index < count; ++index) {
        addObstacle(ObstacleType::Cactus, x, width, height, config_.groundY - height);
        x += width + 2.0;
    }
    return x - config_.worldWidth - 2.0;
}

void RunnerEngine::compactObstacles() noexcept
{
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < obstacleCount_; ++readIndex) {
        const Obstacle& obstacle = obstacles_[readIndex];
        if (obstacle.bounds.x + obstacle.bounds.width < 0.0) {
            continue;
        }
        if (writeIndex != readIndex) {
            obstacles_[writeIndex] = obstacle;
        }
        ++writeIndex;
    }
    obstacleCount_ = writeIndex;
}

void RunnerEngine::checkCollision() noexcept
{
    const Rect player = playerBounds();
    for (std::size_t index = 0; index < obstacleCount_; ++index) {
        const auto &obstacle = obstacles_[index];
        const auto &bounds = obstacle.bounds;
        if (spritesOverlap(sprite(playerSprite()), player.x, player.y, player.width, player.height,
                           sprite(obstacleSprite(obstacle)), bounds.x, bounds.y, bounds.width, bounds.height)) {
            state_ = GameState::GameOver;
            accumulator_ = 0.0;
            return;
        }
    }
}

void RunnerEngine::addObstacle(ObstacleType type, double x, double width, double height,
                               double y, double speedOffset) noexcept
{
    if (obstacleCount_ == obstacles_.size()) {
        return;
    }
    obstacles_[obstacleCount_++] = {type, {x, y, width, height}, speedOffset};
}

double RunnerEngine::obstacleGap() noexcept
{
    const double gapSeconds = config_.minimumGapSeconds
        + (config_.maximumGapSeconds - config_.minimumGapSeconds) * randomUnit(random_);
    return std::max(config_.minimumObstacleGap, speed_ * gapSeconds);
}

SpriteId RunnerEngine::playerSprite() const noexcept
{
    if (state_ == GameState::GameOver) return SpriteId::Dead;
    if (state_ == GameState::Ready || !playerGrounded_) return SpriteId::Stand;
    const bool alternate = static_cast<int>(std::fmod(elapsedSeconds_ * 10.0, 2.0)) != 0;
    if (duckHeld_) return alternate ? SpriteId::DuckB : SpriteId::DuckA;
    return alternate ? SpriteId::RunB : SpriteId::RunA;
}

SpriteId RunnerEngine::obstacleSprite(const Obstacle &obstacle) const noexcept
{
    if (obstacle.type == ObstacleType::Bird)
        return static_cast<int>(std::fmod(elapsedSeconds_ * 6.0, 2.0)) ? SpriteId::BirdDown : SpriteId::BirdUp;
    return obstacle.bounds.height > 40.0 ? SpriteId::CactusLarge : SpriteId::CactusSmall;
}

double RunnerEngine::distanceTravelled() const noexcept { return distanceTravelled_; }
double RunnerEngine::night() const noexcept { return night_; }
bool RunnerEngine::scoreVisible() const noexcept {
    return flashRemaining_ <= 0.0 || static_cast<int>(flashRemaining_ * 5.0) % 2 == 0;
}
void RunnerEngine::updateEffects(double deltaSeconds, std::uint64_t previousScore) noexcept
{
    flashRemaining_ = std::max(0.0, flashRemaining_ - deltaSeconds);
    if (score_ / 100 > previousScore / 100) flashRemaining_ = 1.0;
    nightRemaining_ = std::max(0.0, nightRemaining_ - deltaSeconds);
    if (score_ / 700 > previousScore / 700) nightRemaining_ = 12.0;
    const double target = nightRemaining_ > 0.0 ? 1.0 : 0.0;
    night_ += std::clamp(target - night_, -deltaSeconds / 0.75, deltaSeconds / 0.75);
}

} // namespace dino
