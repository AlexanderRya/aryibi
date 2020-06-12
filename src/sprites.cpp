#include "aryibi/sprites.hpp"
#include "aryibi/sprite_solvers.hpp"

#include <array>

using namespace anton;
namespace aml = anton::math;

namespace aryibi::sprites {

u8 direction::get_direction_texture_index(Direction dir) {
    switch (dir) {
        default:
        case dir_down: return 0;
        case dir_down_right: return 1;
        case dir_right: return 2;
        case dir_up_right: return 3;
        case dir_up: return 4;
        case dir_up_left: return 5;
        case dir_left: return 6;
        case dir_down_left: return 7;
    }
}

TextureChunk TextureChunk::full(renderer::TextureHandle const& tex) {
    return {tex, {{0, 0}, {1, 1}}};
}

void Sprite::join_pieces_from(PieceContainer const& container, aml::Vector2 destination_offset) {
    pieces.reserve(container.size() + pieces.size());
    for (const auto& piece : container) {
        pieces.emplace_back(Piece{piece.source,
                                  {piece.destination.start + destination_offset,
                                   piece.destination.end + destination_offset}});
    }
}

Rect2D Sprite::bounds() const {
    Rect2D rect{{0, 0}, {0, 0}};
    for (const auto& piece : pieces) {
        const Rect2D piece_bounds = piece.destination;
        if (piece_bounds.start.x < rect.start.x)
            rect.start.x = piece_bounds.start.x;
        if (piece_bounds.start.y < rect.start.y)
            rect.start.y = piece_bounds.start.y;
        if (piece_bounds.end.x > rect.end.x)
            rect.end.x = piece_bounds.end.x;
        if (piece_bounds.end.y > rect.end.y)
            rect.end.y = piece_bounds.end.y;
    }
    return rect;
}

Sprite
solve_8_directional(TextureChunk const& chunk, direction::Direction dir, aml::Vector2 target_size) {
    bool is_horizontal = chunk.tex.width() * (chunk.rect.end.x - chunk.rect.start.x) >
                         chunk.tex.height() * (chunk.rect.end.y - chunk.rect.start.y);
    const auto dir_tex_index = (float)direction::get_direction_texture_index(dir);
    const aml::Vector2 directional_sprite_size = (chunk.rect.end - chunk.rect.start) / 8.f;
    /* clang-format off */
    const auto piece = is_horizontal ?
                          Sprite::Piece{
                              {
                                  {chunk.rect.start.x + dir_tex_index * directional_sprite_size.x, chunk.rect.start.y},
                                  {chunk.rect.start.x + (dir_tex_index+1) * directional_sprite_size.x, chunk.rect.end.y}
                              },
                              {
                                  {0, 0},
                                  target_size
                              } }
                          :
                          Sprite::Piece{
                              {
                                  { chunk.rect.start.x, chunk.rect.start.y + dir_tex_index * directional_sprite_size.y},
                                  {chunk.rect.end.x, chunk.rect.start.y + (dir_tex_index+1) * directional_sprite_size.y}
                              },
                              {
                                  {0, 0},
                                  target_size
                              }
                          };
    /* clang-format on */
    return Sprite{chunk.tex, std::vector<Sprite::Piece>{piece}};
}

Sprite
solve_4_directional(TextureChunk const& chunk, direction::Direction dir, aml::Vector2 target_size) {
    bool is_horizontal = chunk.tex.width() * (chunk.rect.end.x - chunk.rect.start.x) >
                         chunk.tex.height() * (chunk.rect.end.y - chunk.rect.start.y);
    const auto dir_tex_index = (float)(direction::get_direction_texture_index(dir) / 2);
    const aml::Vector2 directional_sprite_size = (chunk.rect.end - chunk.rect.start) / 4.f;
    /* clang-format off */
    const auto piece = is_horizontal ?
                       Sprite::Piece{
                           {
                               {chunk.rect.start.x + dir_tex_index * directional_sprite_size.x, chunk.rect.start.y},
                               {chunk.rect.start.x + (dir_tex_index+1) * directional_sprite_size.x, chunk.rect.end.y}
                           },
                           {
                               {0, 0},
                               target_size
                           } }
                                     :
                       Sprite::Piece{
                           {
                               { chunk.rect.start.x, chunk.rect.start.y + dir_tex_index * directional_sprite_size.y},
                               {chunk.rect.end.x, chunk.rect.start.y + (dir_tex_index+1) * directional_sprite_size.y}
                           },
                           {
                               {0, 0},
                               target_size
                           }
                       };
    /* clang-format on */
    return Sprite{chunk.tex, std::vector<Sprite::Piece>{piece}};
}

Sprite solve_normal(TextureChunk const& chunk, aml::Vector2 target_size) {
    aml::Vector2 tex_start{0, 0};
    aml::Vector2 tex_end{1, 0.5f};
    /* clang-format off */
    return Sprite{chunk.tex,
                  std::vector<Sprite::Piece>{
                      Sprite::Piece{
                          {
                              chunk.rect.start,
                              chunk.rect.end
                          },
                          {
                              {0, 0},
                              target_size
                          }
                      }
                  }};
    /* clang-format on */
}

/// Modified RPGMaker A2 algorithm where the X1 tiles are laid out horizontally on the first
/// minitile row.
Sprite solve_rpgmaker_a2(TextureChunk const& tex, Tile8Connections const& connections) {
    static constexpr int rpgmaker_a2_chunk_width = 2;
    static constexpr int rpgmaker_a2_chunk_height = 3;

    /* clang-format off */
    /// Where each minitile is located locally in the RPGMaker A2 layout.
    /// Explanation: https://imgur.com/a/vlRJ9cY
    const std::array<aml::Vector2, 20> layout = {
        /* A1 */ aml::Vector2{2, 0},
        /* A2 */ aml::Vector2{0, 2},
        /* A3 */ aml::Vector2{2, 4},
        /* A4 */ aml::Vector2{2, 2},
        /* A5 */ aml::Vector2{0, 4},
        /* B1 */ aml::Vector2{3, 0},
        /* B2 */ aml::Vector2{3, 2},
        /* B3 */ aml::Vector2{1, 4},
        /* B4 */ aml::Vector2{1, 2},
        /* B5 */ aml::Vector2{3, 4},
        /* C1 */ aml::Vector2{2, 1},
        /* C2 */ aml::Vector2{0, 5},
        /* C3 */ aml::Vector2{2, 3},
        /* C4 */ aml::Vector2{2, 5},
        /* C5 */ aml::Vector2{0, 3},
        /* D1 */ aml::Vector2{3, 1},
        /* D2 */ aml::Vector2{3, 5},
        /* D3 */ aml::Vector2{1, 3},
        /* D4 */ aml::Vector2{1, 5},
        /* D5 */ aml::Vector2{3, 3}
    };
    /* clang-format on */

    Sprite spr;
    for (int minitile = 0; minitile < 4; ++minitile) {
        bool is_connected_vertically, is_connected_horizontally, is_connected_via_corner;
        /* clang-format off */
        std::array<std::tuple<bool, bool, bool>, 4> conn{
            std::tuple{connections.up, connections.left, connections.up_left}, // Top-left minitile
            std::tuple{connections.up, connections.right, connections.up_right}, // Top-right minitile
            std::tuple{connections.down, connections.left, connections.down_left}, // Bottom-left minitile
            std::tuple{connections.down, connections.right, connections.down_right} // Bottom-right minitile
        };
        /* clang-format on */
        is_connected_vertically = std::get<0>(conn[minitile]);
        is_connected_horizontally = std::get<1>(conn[minitile]);
        is_connected_via_corner = std::get<2>(conn[minitile]);

        u8 layout_index = minitile * 5; // Set the minitile position: AX, BX, CX, DX
        switch ((is_connected_via_corner << 2u) | (is_connected_vertically << 1u) |
                (is_connected_horizontally)) {
            case (0b011):
                /* layout_index += 0; */ // X1; All connected except corner
                break;
            case (0b100):
            case (0b000):
                layout_index += 1; // X2; No connections
                break;
            case (0b111):
                layout_index += 2; // X3; All connected
                break;
            case (0b101):
            case (0b001):
                layout_index += 3; // X4; Vertical connection
                break;
            case (0b110):
            case (0b010):
                layout_index += 4; // X5; Horizontal connection
                break;
        }
        const float single_tile_width = 1.f / static_cast<float>(rpgmaker_a2_chunk_width);
        const float single_tile_height = 1.f / static_cast<float>(rpgmaker_a2_chunk_height);

        const auto apply_tex_rect = [&tex](aml::Vector2 const& vec) -> aml::Vector2 {
            return tex.rect.start + vec * (tex.rect.end - tex.rect.start);
        };
        Sprite::Piece piece;
        const auto normalized_start_pos =
            aml::Vector2{(float)layout[layout_index].x, (float)layout[layout_index].y} / 2.f *
            aml::Vector2(single_tile_width, single_tile_height);
        piece.source = {apply_tex_rect(normalized_start_pos),
                        apply_tex_rect(normalized_start_pos +
                                       aml::Vector2(single_tile_width, single_tile_height) / 2.f)};
        piece.destination = {{static_cast<float>(minitile % 2) / 2.f,
                              (1.f - static_cast<float>(minitile / 2)) / 2.f},
                             {static_cast<float>(minitile % 2) / 2.f + .5f,
                              (1.f - static_cast<float>(minitile / 2)) / 2.f + .5f}};
        spr.pieces.emplace_back(Sprite::Piece{piece});
    }

    return spr;
}

} // namespace aryibi::sprites