#ifndef ARYIBI_VULKAN_TYPES_HPP
#define ARYIBI_VULKAN_TYPES_HPP

namespace aryibi::renderer {
    using i8 = signed char;
    using i16 = signed short;
    using i32 = signed int;
    using i64 = signed long long;

    using u8 = unsigned char;
    using u16 = unsigned short;
    using u32 = unsigned int;
    using u64 = unsigned long long;

    using c8 = char;
    using c16 = char16_t;
    using c32 = char32_t;
    using wchar = wchar_t;

    using f32 = float;
    using f64 = double;

    using isize = i64;
    using usize = u64;
} // namespace aryibi::renderer

#endif // ARYIBI_VULKAN_TYPES_HPP
