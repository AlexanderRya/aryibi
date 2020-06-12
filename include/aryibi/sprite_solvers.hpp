#ifndef ARYIBI_SPRITE_SOLVERS_HPP
#define ARYIBI_SPRITE_SOLVERS_HPP

#include <anton/math/vector2.hpp>
#include "sprites.hpp"

namespace aryibi::sprites {

/// Solves an 8-directional sprite atlas contained in a texture chunk.
/// Accepts both horizontally and vertically-stored sprite atlases.
/// The sprites must be in this order (left to right or up to down)
/// down, down_right, right, up_right, up, up_left, left, down_left
Sprite solve_8_directional(TextureChunk const&, direction::Direction dir, anton::math::Vector2 target_size);

/// Solves a 4-directional sprite atlas contained in a texture chunk.
/// Accepts both horizontally and vertically-stored sprite atlases.
/// If diagonal directions are given, the algorithm will choose one (implementation-defined).
/// The sprites must be in this order (left to right or up to down)
/// down, right, up, left
Sprite solve_4_directional(TextureChunk const&, direction::Direction dir, anton::math::Vector2 target_size);

/// Solves a normal tile from a TextureChunk, which literally means "copy the data from this
/// TextureChunk to a Sprite".
Sprite solve_normal(TextureChunk const&, anton::math::Vector2 target_size);

/// Solves a RPGMaker A2 autotile from a set of 8 connections (Depicting what the tile is connected
/// to).
Sprite solve_rpgmaker_a2(TextureChunk const&, Tile8Connections const& connections);

/// Solves a RPGMaker A4 wall autotile from a set of 4 connections (Depicting what the tile is
/// connected to). RPGMaker A4 walls only work with CONVEX shapes, so trying to do an inner corner
/// will result in a broken autotile.
Sprite solve_rpgmaker_a4_wall(TextureChunk const&, Tile4Connections const& connections);

} // namespace aryibi::sprites

#endif // ARYIBI_SPRITE_SOLVERS_HPP
