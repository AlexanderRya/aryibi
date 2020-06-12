#ifndef ARYIBI_SPRITES_HPP
#define ARYIBI_SPRITES_HPP

#include "aryibi/renderer.hpp"
#include <anton/math/math.hpp>

namespace aryibi::sprites {

struct Rect2D {
    anton::math::Vector2 start;
    anton::math::Vector2 end;
};

namespace direction {
enum Direction : anton::u8 {
    dir_none = 0,
    dir_down = 1u << 0u,
    dir_up = 1u << 1u,
    dir_right = 1u << 2u,
    dir_left = 1u << 3u,
    dir_up_right = dir_up | dir_right,
    dir_up_left = dir_up | dir_left,
    dir_down_left = dir_down | dir_left,
    dir_down_right = dir_down | dir_right,
    dir_any = dir_up | dir_right | dir_left | dir_down
};

anton::u8 get_direction_texture_index(Direction);
} // namespace direction

struct TextureChunk {
    renderer::TextureHandle tex;
    /// The rect this chunk is representing, in UV coordinates.
    Rect2D rect;

    /// Returns a chunk with the entirety of the texture given (Rect from 0,0 to 1,1)
    /// This is the same as doing `TextureChunk{texture, {{0,0},{1,1}}}`.
    static TextureChunk full(renderer::TextureHandle const&);
};

struct Sprite {
    /// Texture of the sprite.
    renderer::TextureHandle texture;
    struct Piece {
        /// Where this piece is gathering texture data from, in UV coordinates.
        Rect2D source;
        /// The destination of the source texture. Measured in tiles.
        Rect2D destination;
    };
    using PieceContainer = std::vector<Piece>;
    /// The "pieces" that make up this sprite. A sprite is basically a puzzle of different pieces,
    /// each one having its own texture UV source and destination rect.
    PieceContainer pieces;

    /// Joins the pieces from a sprite to this one and applies an offset to their destination
    /// position.
    void join_pieces_from(PieceContainer const&, anton::math::Vector2 destination_offset);

    /// @returns A rect containing all the pieces (destination rects) of the sprite.
    [[nodiscard]] Rect2D bounds() const;
};

/// Needed for some of the solvers.
struct Tile8Connections {
    bool down = false, down_right = false, right = false, up_right = false, up = false,
         up_left = false, left = false, down_left = false;
};
/// Needed for some of the solvers.
struct Tile4Connections {
    bool down = false, right = false, up = false, left = false;
};

} // namespace aryibi::sprites

#endif // ARYIBI_SPRITES_HPP
