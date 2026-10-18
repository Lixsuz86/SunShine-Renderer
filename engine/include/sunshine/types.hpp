#pragma once

#include <cstdint>

namespace Sunshine {

struct Extent2D {
    uint32_t width{0};
    uint32_t height{0};
};

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    static constexpr Color SunshineAmber() {
        return Color{ 0.95f, 0.60f, 0.15f, 1.0f };
    }

    static constexpr Color CornflowerBlue() {
        return Color{ 0.392f, 0.584f, 0.929f, 1.0f };
    }

    static constexpr Color Black() {
        return Color{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    static constexpr Color White() {
        return Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    static constexpr Color DarkSlate() {
        return Color{ 0.11f, 0.12f, 0.15f, 1.0f };
    }
};

} // namespace Sunshine
