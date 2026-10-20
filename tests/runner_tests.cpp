#include "../src/core/runner.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr double Step = 1.0 / 120.0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::fprintf(stderr, "runner test failed at line %d: %s\n", line, expression);
        std::abort();
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void advance(dino::RunnerEngine& engine, int steps, dino::Input input = {})
{
    for (int index = 0; index < steps; ++index) {
        engine.update(Step, input);
        input.jumpPressed = false;
    }
}

void testStateAndProgress()
{
    dino::RunnerEngine engine(7);
    CHECK(engine.state() == dino::GameState::Ready);
    engine.update(1.0);
    CHECK(engine.score() == 0);

    engine.start();
    CHECK(engine.state() == dino::GameState::Running);
    advance(engine, 120);
    const auto score = engine.score();
    const auto elapsed = engine.elapsedSeconds();
    CHECK(score > 0);
    CHECK(elapsed > 0.99 && elapsed < 1.01);

    advance(engine, 120);
    CHECK(engine.score() > score);
    CHECK(engine.elapsedSeconds() > elapsed);
    CHECK(engine.state() == dino::GameState::Running);
}

void testJumpHoldAndFastFall()
{
    dino::RunnerEngine held(3);
    dino::RunnerEngine tapped(3);
    held.start();
    tapped.start();

    held.update(Step, {.jumpPressed = true, .jumpHeld = true});
    tapped.update(Step, {.jumpPressed = true});
    double heldHighest = held.playerBounds().y;
    double tappedHighest = tapped.playerBounds().y;
    for (int index = 0; index < 32; ++index) {
        held.update(Step, {.jumpHeld = true});
        tapped.update(Step);
        heldHighest = std::min(heldHighest, held.playerBounds().y);
        tappedHighest = std::min(tappedHighest, tapped.playerBounds().y);
    }
    CHECK(heldHighest < tappedHighest);

    dino::RunnerEngine normalFall(11);
    dino::RunnerEngine fastFall(11);
    normalFall.start();
    fastFall.start();
    advance(normalFall, 1, {.jumpPressed = true});
    advance(fastFall, 1, {.jumpPressed = true});
    advance(normalFall, 12);
    advance(fastFall, 12);
    normalFall.update(Step);
    fastFall.update(Step, {.duckHeld = true});
    CHECK(fastFall.playerVelocityY() > normalFall.playerVelocityY());
}

void testDuckBounds()
{
    dino::RunnerEngine engine(5);
    const auto standing = engine.playerBounds();
    engine.start();
    engine.update(Step, {.duckHeld = true});
    const auto ducking = engine.playerBounds();
    CHECK(ducking.height < standing.height);
    CHECK(ducking.y > standing.y);
}

void testDeterministicObstaclesAndBoundedStorage()
{
    dino::RunnerEngine first(0x12345678u);
    dino::RunnerEngine second(0x12345678u);
    first.start();
    second.start();
    for (int frame = 0; frame < 240; ++frame) {
        first.update(1.0 / 60.0);
        second.update(1.0 / 60.0);
        CHECK(first.obstacles().size() <= dino::RunnerEngine::MaxObstacles);
        CHECK(first.obstacles().size() == second.obstacles().size());
        for (std::size_t index = 0; index < first.obstacles().size(); ++index) {
            const auto& a = first.obstacles()[index];
            const auto& b = second.obstacles()[index];
            CHECK(a.type == b.type);
            CHECK(std::abs(a.bounds.x - b.bounds.x) < 1e-9);
            CHECK(std::abs(a.bounds.y - b.bounds.y) < 1e-9);
            CHECK(std::abs(a.bounds.width - b.bounds.width) < 1e-9);
        }
    }
}

void testCollisionEndsRun()
{
    dino::RunnerEngine engine(9);
    engine.start();
    for (int frame = 0; frame < 600 && engine.state() == dino::GameState::Running; ++frame) {
        engine.update(1.0 / 60.0);
    }
    CHECK(engine.state() == dino::GameState::GameOver);
}

void testExtremeConfigIsBounded()
{
    dino::Config config;
    config.fixedStepSeconds = 1.0;
    config.maxFrameDeltaSeconds = 128.0;
    config.initialSpeed = 1000.0;
    config.maxSpeed = 1000.0;
    config.minimumObstacleGap = 1.0;
    config.minimumGapSeconds = 0.001;
    config.maximumGapSeconds = 0.001;

    dino::RunnerEngine engine(19, config);
    engine.start();
    engine.update(128.0);
    CHECK(engine.elapsedSeconds() == 64.0);
    CHECK(engine.obstacles().size() <= dino::RunnerEngine::MaxObstacles);
}

void testReferenceSceneryAndDifficulty() {
    dino::Config config;
    config.playerX = -10000.0;
    config.initialSpeed = config.maxSpeed = 600.0;
    dino::RunnerEngine engine(17, config);
    engine.start();
    advance(engine, 359);
    CHECK(engine.obstacles().empty());
    advance(engine, 2);
    CHECK(!engine.obstacles().empty());
    CHECK(engine.obstacles().size() <= 3);
    bool sawBird = false, sawLow = false, sawMiddle = false, sawHigh = false;
    bool sawFlash = false;
    for (int i = 0; i < 6000; ++i) {
        engine.update(Step);
        sawFlash = sawFlash || !engine.scoreVisible();
        for (const auto &obstacle : engine.obstacles()) {
            if (obstacle.type != dino::ObstacleType::Bird) continue;
            sawBird = true;
            const double bottom = obstacle.bounds.y + obstacle.bounds.height;
            sawLow = sawLow || bottom == config.groundY;
            sawMiddle = sawMiddle || bottom == config.groundY - 27.0;
            sawHigh = sawHigh || bottom == config.groundY - 52.0;
        }
    }
    CHECK(engine.state() == dino::GameState::Running);
    CHECK(sawBird && sawLow && sawMiddle && sawHigh);
    CHECK(sawFlash);
    CHECK(engine.night() > 0.99);
    advance(engine, 1800);
    CHECK(engine.night() < 0.01);
    CHECK(engine.score() == static_cast<std::uint64_t>(engine.distanceTravelled() * config.scorePerUnit));
}

void testPixelCollisionAndDuckPosture() {
    constexpr std::string_view rows[]{".#", "##"};
    constexpr std::string_view dot[]{"#"};
    const dino::Sprite corner{rows, 4, 4}, point{dot, 2, 2};
    CHECK(!dino::spritesOverlap(corner, 0, 0, 4, 4, point, 0, 0, 2, 2));
    CHECK(dino::spritesOverlap(corner, 0, 0, 4, 4, point, 1.5, 0, 2, 2));
    CHECK(!dino::spritesOverlap(corner, 0, 0, 4, 4, point, 4, 0, 2, 2));
    dino::RunnerEngine engine(3);
    engine.start();
    engine.update(Step, {.jumpPressed = true, .duckHeld = true});
    CHECK(engine.playerGrounded());
    CHECK(engine.playerBounds().width == engine.config().duckWidth);
    CHECK(engine.playerSprite() == dino::SpriteId::DuckA);
    engine.update(Step, {.jumpPressed = true});
    CHECK(!engine.playerGrounded());
    CHECK(engine.playerBounds().width == engine.config().playerWidth);
}

void testFrameRateIndependence() {
    dino::RunnerEngine fast(42), slow(42);
    fast.start(); slow.start();
    for (int i = 0; i < 360; ++i) fast.update(1.0 / 120.0);
    for (int i = 0; i < 180; ++i) slow.update(1.0 / 60.0);
    CHECK(fast.score() == slow.score());
    CHECK(std::abs(fast.distanceTravelled() - slow.distanceTravelled()) < 1e-8);
    CHECK(fast.obstacles().size() == slow.obstacles().size());
}

} // namespace

int main()
{
    testReferenceSceneryAndDifficulty();
    testPixelCollisionAndDuckPosture();
    testFrameRateIndependence();
    testStateAndProgress();
    testJumpHoldAndFastFall();
    testDuckBounds();
    testDeterministicObstaclesAndBoundedStorage();
    testCollisionEndsRun();
    testExtremeConfigIsBounded();
}
