#pragma once
// Data from https://www.graphics.cornell.edu/online/box/data.html

#include <cstdint>

struct Point {
    float x;
    float y;
    float z;
};

// Linear sRGB color, simply sampled from the spectral data at 612, 548 and 464 nm
struct Color {
    float r;
    float g;
    float b;
};

// Identification for purposes of merging planes under the same AS
enum class CornellObject : uint32_t {
    Floor,
    Ceiling,
    Light,
    BackWall,
    RightWall,
    LeftWall,
    ShortBlock,
    TallBlock,

    NObjects
};

// All surfaces are represented as bidirectional planes
struct Plane {
    CornellObject objectId;
    Point p0;
    Point p1;
    Point p2;
    Point p3;
    Color reflectance;
    Color emission;
};

static const Color cornellWhite = { 0.755, 0.748, 0.751 };

static const Plane cornellBox[] = {
    {
        CornellObject::Floor,
        { 552.8, 0.0, 0.0 },
        { 0.0, 0.0, 0.0 },
        { 0.0, 0.0, 559.2 },
        { 549.6, 0.0, 559.2 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::Ceiling,
        { 556.0, 548.8, 0.0 },
        { 556.0, 548.8, 559.2 },
        { 0.0, 548.8, 559.2 },
        { 0.0, 548.8, 0.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::Light,
        // Slightly offset downwards compared to the original data
        { 343.0, 548.7, 227.0 },
        { 343.0, 548.7, 332.0 },
        { 213.0, 548.7, 332.0 },
        { 213.0, 548.7, 227.0 },
        { 0.78, 0.78, 0.78 },
        { 15.94, 11.65, 5.12 },
    },
    {
        CornellObject::BackWall,
        { 549.6, 0.0, 559.2 },
        { 0.0, 0.0, 559.2 },
        { 0.0, 548.0, 559.2 },
        { 556.0, 548.0, 559.2 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::RightWall,
        { 0.0, 0.0, 559.2 },
        { 0.0, 0.0, 0.0 },
        { 0.0, 548.8, 0.0 },
        { 0.0, 548.8, 559.2 },
        { 0.136, 0.406, 0.107 },
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::LeftWall,
        { 552.8, 0.0, 0.0 },
        { 549.6, 0.0, 559.2 },
        { 556.0, 548.8, 559.2 },
        { 556.0, 548.8, 0.0 },
        { 0.443, 0.061, 0.062 },
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::ShortBlock,
        { 130.0, 165.0, 65.0 },
        { 82.0, 165.0, 225.0 },
        { 240.0, 165.0, 272.0 },
        { 290.0, 165.0, 114.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::ShortBlock,
        { 290.0, 0.0, 114.0 },
        { 290.0, 165.0, 114.0 },
        { 240.0, 165.0, 272.0 },
        { 240.0, 0.0, 272.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::ShortBlock,
        { 130.0, 0.0, 65.0 },
        { 130.0, 165.0, 65.0 },
        { 290.0, 165.0, 114.0 },
        { 290.0, 0.0, 114.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::ShortBlock,
        { 82.0, 0.0, 225.0 },
        { 82.0, 165.0, 225.0 },
        { 130.0, 165.0, 65.0 },
        { 130.0, 0.0, 65.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::ShortBlock,
        { 240.0, 0.0, 272.0 },
        { 240.0, 165.0, 272.0 },
        { 82.0, 165.0, 225.0 },
        { 82.0, 0.0, 225.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::TallBlock,
        { 423.0, 330.0, 247.0 },
        { 265.0, 330.0, 296.0 },
        { 314.0, 330.0, 456.0 },
        { 472.0, 330.0, 406.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::TallBlock,
        { 423.0, 0.0, 247.0 },
        { 423.0, 330.0, 247.0 },
        { 472.0, 330.0, 406.0 },
        { 472.0, 0.0, 406.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::TallBlock,
        { 472.0, 0.0, 406.0 },
        { 472.0, 330.0, 406.0 },
        { 314.0, 330.0, 456.0 },
        { 314.0, 0.0, 456.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::TallBlock,
        { 314.0, 0.0, 456.0 },
        { 314.0, 330.0, 456.0 },
        { 265.0, 330.0, 296.0 },
        { 265.0, 0.0, 296.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
    {
        CornellObject::TallBlock,
        { 265.0, 0.0, 296.0 },
        { 265.0, 330.0, 296.0 },
        { 423.0, 330.0, 247.0 },
        { 423.0, 0.0, 247.0 },
        cornellWhite,
        { 0.0, 0.0, 0.0 },
    },
};
