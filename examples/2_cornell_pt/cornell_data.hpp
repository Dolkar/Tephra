#pragma once
// Data from https://www.graphics.cornell.edu/online/box/data.html

#include "trace_shared.h"

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
    // Linearized sRGB color converted from reference spectra under the D65 illuminant
    Color reflectance;
    Color emission;
};

static const Color cornellWhite = { 0.736f, 0.736f, 0.739f };

static const Plane cornellBox[] = {
    {
        CornellObject::Floor,
        { 552.8f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 559.2f },
        { 549.6f, 0.0f, 559.2f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::Ceiling,
        { 556.0f, 548.8f, 0.0f },
        { 556.0f, 548.8f, 559.2f },
        { 0.0f, 548.8f, 559.2f },
        { 0.0f, 548.8f, 0.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::Light,
        // Slightly offset downwards compared to the original data
        { 343.0f, 548.7f, 227.0f },
        { 343.0f, 548.7f, 332.0f },
        { 213.0f, 548.7f, 332.0f },
        { 213.0f, 548.7f, 227.0f },
        { 0.78f, 0.78f, 0.78f },
        { 17.74f, 11.37f, 3.31f },
    },
    {
        CornellObject::BackWall,
        { 549.6f, 0.0f, 559.2f },
        { 0.0f, 0.0f, 559.2f },
        { 0.0f, 548.8f, 559.2f },
        { 556.0f, 548.8f, 559.2f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::RightWall,
        { 0.0f, 0.0f, 559.2f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 548.8f, 0.0f },
        { 0.0f, 548.8f, 559.2f },
        { 0.068f, 0.398f, 0.087f },
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::LeftWall,
        { 552.8f, 0.0f, 0.0f },
        { 549.6f, 0.0f, 559.2f },
        { 556.0f, 548.8f, 559.2f },
        { 556.0f, 548.8f, 0.0f },
        { 0.494f, 0.049f, 0.051f },
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::ShortBlock,
        { 130.0f, 165.0f, 65.0f },
        { 82.0f, 165.0f, 225.0f },
        { 240.0f, 165.0f, 272.0f },
        { 290.0f, 165.0f, 114.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::ShortBlock,
        { 290.0f, 0.0f, 114.0f },
        { 290.0f, 165.0f, 114.0f },
        { 240.0f, 165.0f, 272.0f },
        { 240.0f, 0.0f, 272.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::ShortBlock,
        { 130.0f, 0.0f, 65.0f },
        { 130.0f, 165.0f, 65.0f },
        { 290.0f, 165.0f, 114.0f },
        { 290.0f, 0.0f, 114.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::ShortBlock,
        { 82.0f, 0.0f, 225.0f },
        { 82.0f, 165.0f, 225.0f },
        { 130.0f, 165.0f, 65.0f },
        { 130.0f, 0.0f, 65.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::ShortBlock,
        { 240.0f, 0.0f, 272.0f },
        { 240.0f, 165.0f, 272.0f },
        { 82.0f, 165.0f, 225.0f },
        { 82.0f, 0.0f, 225.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::TallBlock,
        { 423.0f, 330.0f, 247.0f },
        { 265.0f, 330.0f, 296.0f },
        { 314.0f, 330.0f, 456.0f },
        { 472.0f, 330.0f, 406.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::TallBlock,
        { 423.0f, 0.0f, 247.0f },
        { 423.0f, 330.0f, 247.0f },
        { 472.0f, 330.0f, 406.0f },
        { 472.0f, 0.0f, 406.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::TallBlock,
        { 472.0f, 0.0f, 406.0f },
        { 472.0f, 330.0f, 406.0f },
        { 314.0f, 330.0f, 456.0f },
        { 314.0f, 0.0f, 456.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::TallBlock,
        { 314.0f, 0.0f, 456.0f },
        { 314.0f, 330.0f, 456.0f },
        { 265.0f, 330.0f, 296.0f },
        { 265.0f, 0.0f, 296.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
    {
        CornellObject::TallBlock,
        { 265.0f, 0.0f, 296.0f },
        { 265.0f, 330.0f, 296.0f },
        { 423.0f, 330.0f, 247.0f },
        { 423.0f, 0.0f, 247.0f },
        cornellWhite,
        { 0.0f, 0.0f, 0.0f },
    },
};
