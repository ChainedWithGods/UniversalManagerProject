#include "SDKGenerator_Containers.h"
#include "SDKGenerator_LayoutRules.h"

namespace SDKGenerator {

std::string GetUtfNHeaderContent() {
    return R"AVALONIUM_UTFN(#pragma once

#if (defined(_MSC_VER))
#pragma warning (push, 2)
#pragma warning(disable : 4365)
#pragma warning(disable : 4710)
#pragma warning(disable : 4711)
#elif (defined(__CLANG__) || defined(__GNUC__))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <string>
#include <limits>
#include <cstdint>
#include <type_traits>

#ifdef _DEBUG
#include <stdexcept>
#endif

#if (defined(_MSC_VER))
#pragma warning (pop)
#elif (defined(__CLANG__) || defined(__GNUC__))
#pragma GCC diagnostic pop
#endif

#if (defined(_MSC_VER))
#pragma warning (push)
#pragma warning (disable: 4514)
#pragma warning (disable: 4820)
#pragma warning(disable : 4127)
#pragma warning(disable : 5045)
#pragma warning(disable : 5246)
#elif (defined(__CLANG__) || defined(__GNUC__))
#endif

#ifdef __cpp_constexpr
#define UTF_CONSTEXPR constexpr
#else
#define UTF_CONSTEXPR
#endif

#ifdef __cpp_if_constexpr
#define UTF_IF_CONSTEXPR constexpr
#else
#define UTF_IF_CONSTEXPR
#endif

#if (defined(__cpp_constexpr) && __cpp_constexpr >= 201304L)
#define UTF_CONSTEXPR14 constexpr
#else
#define UTF_CONSTEXPR14
#endif

#if (defined(__cpp_constexpr) && __cpp_constexpr >= 201603L)
#define UTF_CONSTEXPR17 constexpr
#else
#define UTF_CONSTEXPR17 inline
#endif

#if (defined(__cpp_constexpr) && __cpp_constexpr >= 201907L)
#define UTF_CONSTEXPR20 constexpr
#else
#define UTF_CONSTEXPR20 inline
#endif

#if (defined(__cpp_constexpr) && __cpp_constexpr >= 202211L)
#define UTF_CONSTEXPR23 constexpr
#else
#define UTF_CONSTEXPR23 inline
#endif

#if (defined(__cpp_constexpr) && __cpp_constexpr >= 202406L)
#define UTF_CONSTEXPR26 constexpr
#else
#define UTF_CONSTEXPR26 inline
#endif

#ifdef __cpp_nodiscard
#define UTF_NODISCARD [[nodiscard]]
#else
#define UTF_NODISCARD
#endif

namespace UtfN
{
#if defined(__cpp_char8_t)
	typedef char8_t utf_cp8_t;
	typedef char16_t utf_cp16_t;
	typedef char32_t utf_cp32_t;
#elif defined(__cpp_unicode_characters)
	typedef unsigned char utf_cp8_t;
	typedef char16_t utf_cp16_t;
	typedef char32_t utf_cp32_t;
#else
	typedef unsigned char utf_cp8_t;
	typedef uint16_t utf_cp16_t;
	typedef uint32_t utf_cp32_t;
#endif

	namespace UtfImpl
	{
		namespace Utils
		{
			template<typename value_type, typename flag_type>
			UTF_CONSTEXPR UTF_NODISCARD
				bool IsFlagSet(value_type Value, flag_type Flag) noexcept
			{
				return (Value & Flag) == Flag;
			}

			template<typename value_type, typename flag_type>
			UTF_CONSTEXPR UTF_NODISCARD
				value_type GetWithClearedFlag(value_type Value, flag_type Flag) noexcept
			{
				return static_cast<value_type>(Value & static_cast<flag_type>(~Flag));
			}

			template<typename target_type, typename current_type>
			UTF_CONSTEXPR UTF_NODISCARD
				auto ForceCastIfMissmatch(current_type&& Arg) -> std::enable_if_t<std::is_same<std::decay_t<target_type>, std::decay_t<current_type>>::value, current_type>
			{
				return static_cast<current_type>(Arg);
			}

			template<typename target_type, typename current_type>
			UTF_CONSTEXPR UTF_NODISCARD
				auto ForceCastIfMissmatch(current_type&& Arg) -> std::enable_if_t<!std::is_same<std::decay_t<target_type>, std::decay_t<current_type>>::value, target_type>
			{
				return reinterpret_cast<target_type>(Arg);
			}
		}

		UTF_CONSTEXPR bool IsWCharUtf32 = sizeof(wchar_t) == 0x4;

		UTF_CONSTEXPR utf_cp32_t MaxValidUnicodeChar = 0x10FFFF;

		namespace Utf8
		{

			UTF_CONSTEXPR utf_cp32_t Max1ByteValue = (1 <<  7) - 1;
			UTF_CONSTEXPR utf_cp32_t Max2ByteValue = (1 << 11) - 1;
			UTF_CONSTEXPR utf_cp32_t Max3ByteValue = (1 << 16) - 1;
			UTF_CONSTEXPR utf_cp32_t Max4ByteValue = 0x10FFFF;

			UTF_CONSTEXPR utf_cp8_t FollowupByteMask = 0b1000'0000;
			UTF_CONSTEXPR utf_cp8_t FollowupByteDataMask = 0b0011'1111;
			UTF_CONSTEXPR utf_cp8_t NumDataBitsInFollowupByte = 0x6;

			UTF_CONSTEXPR utf_cp8_t TwoByteFlag = 0b1100'0000;
			UTF_CONSTEXPR utf_cp8_t ThreeByteFlag = 0b1110'0000;
			UTF_CONSTEXPR utf_cp8_t FourByteFlag = 0b1111'0000;

			UTF_CONSTEXPR UTF_NODISCARD
				bool IsValidFollowupCodepoint(const utf_cp8_t Codepoint) noexcept
			{

				return (Codepoint & 0b1100'0000) == FollowupByteMask;
			}

			template<int ByteSize>
			UTF_CONSTEXPR UTF_NODISCARD
				bool IsValidUtf8Sequence(const utf_cp8_t FirstCp, const utf_cp8_t SecondCp, const utf_cp8_t ThirdCp, const utf_cp8_t FourthCp) noexcept
			{
				switch (ByteSize)
				{
				case 1:
				{
					return SecondCp == 0 && ThirdCp == 0 && FourthCp == 0;
				}
				case 4:
				{
					const bool bIsOverlongEncoding = Utils::GetWithClearedFlag(FirstCp, ~Utf8::TwoByteFlag) != 0 && SecondCp == Utf8::FollowupByteMask;
					return !bIsOverlongEncoding && IsValidFollowupCodepoint(SecondCp) && IsValidFollowupCodepoint(ThirdCp) && IsValidFollowupCodepoint(FourthCp);
				}
				case 3:
				{
					const bool bIsOverlongEncoding = Utils::GetWithClearedFlag(FirstCp, ~Utf8::ThreeByteFlag) != 0 && SecondCp == Utf8::FollowupByteMask;
					return !bIsOverlongEncoding && IsValidFollowupCodepoint(SecondCp) && IsValidFollowupCodepoint(ThirdCp) && FourthCp == 0;
				}
				case 2:
				{
					const bool bIsOverlongEncoding = Utils::GetWithClearedFlag(FirstCp, ~Utf8::FourByteFlag) != 0 && SecondCp == Utf8::FollowupByteMask;
					return !bIsOverlongEncoding && IsValidFollowupCodepoint(SecondCp) && ThirdCp == 0 && FourthCp == 0;
				}
				default:
				{
					return false;
					break;
				}
				}
			}
		}

		namespace Utf16
		{

			UTF_CONSTEXPR utf_cp16_t HighSurrogateRangeStart = 0xD800;
			UTF_CONSTEXPR utf_cp16_t LowerSurrogateRangeStart = 0xDC00;

			UTF_CONSTEXPR utf_cp32_t SurrogatePairOffset = 0x10000;

			UTF_CONSTEXPR utf_cp32_t SurrogateRangeLowerBounds = 0xD800;
			UTF_CONSTEXPR utf_cp32_t SurrogateRangeUpperBounds = 0xDFFF;

			UTF_CONSTEXPR UTF_NODISCARD
				bool IsHighSurrogate(const utf_cp16_t Codepoint) noexcept
			{

				return Codepoint >= HighSurrogateRangeStart && Codepoint < LowerSurrogateRangeStart;
			}
			UTF_CONSTEXPR UTF_NODISCARD
				bool IsLowSurrogate(const utf_cp16_t Codepoint) noexcept
			{

				return Codepoint >= LowerSurrogateRangeStart && Codepoint <= SurrogateRangeUpperBounds;
			}

			UTF_CONSTEXPR UTF_NODISCARD
				bool IsValidUnicodeChar(const utf_cp16_t LowerCodepoint, const utf_cp16_t UpperCodepoint) noexcept
			{
				const bool IsValidHighSurrogate = IsHighSurrogate(UpperCodepoint);
				const bool IsValidLowSurrogate = IsLowSurrogate(LowerCodepoint);

				if (IsValidHighSurrogate)
					return IsValidLowSurrogate;

				return !IsValidLowSurrogate && !IsHighSurrogate(LowerCodepoint) && !IsLowSurrogate(UpperCodepoint);
			}
		}

		namespace Utf32
		{

			UTF_CONSTEXPR UTF_NODISCARD
				bool IsValidUnicodeChar(const utf_cp32_t Codepoint) noexcept
			{

				return Codepoint < MaxValidUnicodeChar && (Codepoint < Utf16::SurrogateRangeLowerBounds || Codepoint > Utf16::SurrogateRangeUpperBounds);
			}
		}

		namespace Iterator
		{
			template<typename child_type>
			class utf_char_iterator_base_child_acessor
			{
			private:
				template<class child_iterator_type, typename codepoint_iterator_type, typename utf_char_type>
				friend class utf_char_iterator_base;

			private:
				static UTF_CONSTEXPR
					void ReadChar(child_type* This)
				{
					return This->ReadChar();
				}
			};

			template<class child_iterator_type, typename codepoint_iterator_type, typename utf_char_type>
			class utf_char_iterator_base
			{
			public:
				UTF_CONSTEXPR utf_char_iterator_base(codepoint_iterator_type Begin, codepoint_iterator_type End)
					: CurrentIterator(Begin), NextCharStartIterator(Begin), EndIterator(End)
				{
					utf_char_iterator_base_child_acessor<child_iterator_type>::ReadChar(static_cast<child_iterator_type*>(this));
				}

				template<typename container_type,
					typename = decltype(std::begin(std::declval<container_type>())),
					typename = decltype(std::end(std::declval<container_type>())),
					typename iterator_deref_type = decltype(*std::end(std::declval<container_type>())),
					typename = typename std::enable_if<sizeof(typename std::decay<iterator_deref_type>::type) == utf_char_type::GetCodepointSize()>::type
				>
				explicit UTF_CONSTEXPR utf_char_iterator_base(container_type& Container)
					: CurrentIterator(std::begin(Container)), NextCharStartIterator(std::begin(Container)), EndIterator(std::end(Container))
				{
					utf_char_iterator_base_child_acessor<child_iterator_type>::ReadChar(static_cast<child_iterator_type*>(this));
				}

			public:
				UTF_CONSTEXPR inline
					child_iterator_type& operator++()
				{

					CurrentIterator = NextCharStartIterator;

					utf_char_iterator_base_child_acessor<child_iterator_type>::ReadChar(static_cast<child_iterator_type*>(this));

					return *static_cast<child_iterator_type*>(this);
				}

			public:
				UTF_CONSTEXPR inline
					utf_char_type operator*() const
				{
					return CurrentChar;
				}

				UTF_CONSTEXPR inline bool operator==(const child_iterator_type& Other) const
				{
					return CurrentIterator == Other.CurrentIterator;
				}
				UTF_CONSTEXPR inline
					bool operator!=(const child_iterator_type& Other) const
				{
					return CurrentIterator != Other.CurrentIterator;
				}

				UTF_CONSTEXPR inline
					explicit operator bool() const
				{
					return this->CurrentIterator != this->EndIterator;
				}

			public:
				UTF_CONSTEXPR inline
					child_iterator_type begin()
				{
					return *static_cast<child_iterator_type*>(this);
				}

				UTF_CONSTEXPR inline
					child_iterator_type end()
				{
					return child_iterator_type(EndIterator, EndIterator);
				}

			protected:
				codepoint_iterator_type CurrentIterator;
				codepoint_iterator_type NextCharStartIterator;
				codepoint_iterator_type EndIterator;

				utf_char_type CurrentChar;
			};
		}
	}

	struct utf8_bytes
	{
		utf_cp8_t Codepoints[4] = { 0 };
	};

	struct utf16_pair
	{
		utf_cp16_t Lower = 0;
		utf_cp16_t Upper = 0;
	};

	UTF_CONSTEXPR inline
		bool operator==(const utf8_bytes Left, const utf8_bytes Right) noexcept
	{
		return Left.Codepoints[0] == Right.Codepoints[0]
			&& Left.Codepoints[1] == Right.Codepoints[1]
			&& Left.Codepoints[2] == Right.Codepoints[2]
			&& Left.Codepoints[3] == Right.Codepoints[3];
	}
	UTF_CONSTEXPR inline
		bool operator!=(const utf8_bytes Left, const utf8_bytes Right) noexcept
	{
		return !(Left == Right);
	}

	UTF_CONSTEXPR inline
		bool operator==(const utf16_pair Left, const utf16_pair Right) noexcept
	{
		return Left.Upper == Right.Upper && Left.Lower == Right.Lower;
	}
	UTF_CONSTEXPR inline
		bool operator!=(const utf16_pair Left, const utf16_pair Right) noexcept
	{
		return !(Left == Right);
	}

	enum class UtfEncodingType
	{
		Invalid,
		Utf8,
		Utf16,
		Utf32
	};

	template<UtfEncodingType Encoding>
	struct utf_char;

	typedef utf_char<UtfEncodingType::Utf8> utf_char8;
	typedef utf_char<UtfEncodingType::Utf16> utf_char16;
	typedef utf_char<UtfEncodingType::Utf32> utf_char32;

	template<>
	struct utf_char<UtfEncodingType::Utf8>
	{
		utf8_bytes Char = { 0 };

	public:
		UTF_CONSTEXPR utf_char() = default;
		UTF_CONSTEXPR utf_char(utf_char&&) = default;
		UTF_CONSTEXPR utf_char(const utf_char&) = default;

		UTF_CONSTEXPR utf_char(utf8_bytes InChar) noexcept;

		template<typename char_type, typename = decltype(ParseUtf8CharFromStr(std::declval<const char_type*>()))>
		UTF_CONSTEXPR utf_char(const char_type* SingleCharString) noexcept;

	public:
		UTF_CONSTEXPR utf_char& operator=(utf_char&&) = default;
		UTF_CONSTEXPR utf_char& operator=(const utf_char&) = default;

		UTF_CONSTEXPR utf_char& operator=(utf8_bytes inBytse) noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD       utf_cp8_t&  operator[](const uint8_t Index);
		UTF_CONSTEXPR UTF_NODISCARD const utf_cp8_t&  operator[](const uint8_t Index) const;

		UTF_CONSTEXPR UTF_NODISCARD bool operator==(utf_char8 Other) const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD bool operator!=(utf_char8 Other) const noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD utf_char8 GetAsUtf8() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char16 GetAsUtf16() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char32 GetAsUtf32() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD utf8_bytes Get() const;

		UTF_CONSTEXPR UTF_NODISCARD UtfEncodingType GetEncoding() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD uint8_t GetNumCodepoints() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD static uint8_t GetCodepointSize() noexcept;
	};

	template<>
	struct utf_char<UtfEncodingType::Utf16>
	{
		utf16_pair Char = { 0 };

	public:
		UTF_CONSTEXPR utf_char() = default;
		UTF_CONSTEXPR utf_char(utf_char&&) = default;
		UTF_CONSTEXPR utf_char(const utf_char&) = default;

		UTF_CONSTEXPR utf_char(utf16_pair InChar) noexcept;

		template<typename char_type, typename = decltype(ParseUtf16CharFromStr(std::declval<const char_type*>()))>
		UTF_CONSTEXPR utf_char(const char_type* SingleCharString) noexcept;

	public:
		UTF_CONSTEXPR utf_char& operator=(utf_char&&) = default;
		UTF_CONSTEXPR utf_char& operator=(const utf_char&) = default;

		UTF_CONSTEXPR utf_char& operator=(utf16_pair inBytse) noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD bool operator==(utf_char16 Other) const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD bool operator!=(utf_char16 Other) const noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD utf_char8 GetAsUtf8() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char16 GetAsUtf16() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char32 GetAsUtf32() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD utf16_pair Get() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD UtfEncodingType GetEncoding() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD uint8_t GetNumCodepoints() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD static uint8_t GetCodepointSize() noexcept;
	};

	template<>
	struct utf_char<UtfEncodingType::Utf32>
	{
		utf_cp32_t Char = { 0 };

	public:
		UTF_CONSTEXPR utf_char() = default;
		UTF_CONSTEXPR utf_char(utf_char&&) = default;
		UTF_CONSTEXPR utf_char(const utf_char&) = default;

		UTF_CONSTEXPR utf_char(utf_cp32_t InChar) noexcept;

		template<typename char_type, typename = decltype(ParseUtf32CharFromStr(std::declval<const char_type*>()))>
		UTF_CONSTEXPR utf_char(const char_type* SingleCharString) noexcept;

	public:
		UTF_CONSTEXPR utf_char& operator=(utf_char&&) = default;
		UTF_CONSTEXPR utf_char& operator=(const utf_char&) = default;

		UTF_CONSTEXPR utf_char& operator=(utf_cp32_t inBytse) noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD bool operator==(utf_char32 Other) const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD bool operator!=(utf_char32 Other) const noexcept;

	public:
		UTF_CONSTEXPR UTF_NODISCARD utf_char8 GetAsUtf8() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char16 GetAsUtf16() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD utf_char32 GetAsUtf32() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD utf_cp32_t Get() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD UtfEncodingType GetEncoding() const noexcept;
		UTF_CONSTEXPR UTF_NODISCARD uint8_t GetNumCodepoints() const noexcept;

		UTF_CONSTEXPR UTF_NODISCARD static uint8_t GetCodepointSize() noexcept;
	};

	UTF_CONSTEXPR UTF_NODISCARD
		uint8_t GetUtf8CharLenght(const utf_cp8_t C) noexcept
	{
		using namespace UtfImpl;

		if ((C & 0b1000'0000) == 0)
		{
			return 0x1;
		}
		else if (Utils::IsFlagSet(C, Utf8::FourByteFlag))
		{
			return 0x4;
		}
		else if (Utils::IsFlagSet(C, Utf8::ThreeByteFlag))
		{
			return 0x3;
		}
		else if (Utils::IsFlagSet(C, Utf8::TwoByteFlag))
		{
			return 0x2;
		}
		else
		{

			return 0x0;
		}
	}

	UTF_CONSTEXPR UTF_NODISCARD
		uint8_t GetUtf16CharLenght(const utf_cp16_t UpperCodepoint) noexcept
	{
		if (UtfImpl::Utf16::IsHighSurrogate(UpperCodepoint))
			return 0x2;

		return 0x1;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 Utf32ToUtf16Pair(const utf_char32 Character) noexcept
	{
		using namespace UtfImpl;

		if (!Utf32::IsValidUnicodeChar(Character.Char))
			return utf16_pair{};

		utf16_pair RetCharPair;

		if (Character.Char > USHRT_MAX)
		{
			const utf_cp32_t PreparedCodepoint = Character.Char - Utf16::SurrogatePairOffset;

			RetCharPair.Upper = (PreparedCodepoint >> 10) & 0b1111111111;
			RetCharPair.Lower = PreparedCodepoint & 0b1111111111;

			RetCharPair.Upper += Utf16::HighSurrogateRangeStart;
			RetCharPair.Lower += Utf16::LowerSurrogateRangeStart;

			return RetCharPair;
		}

		RetCharPair.Lower = static_cast<utf_cp16_t>(Character.Char);

		return RetCharPair;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char32 Utf16PairToUtf32(const utf_char16 Character) noexcept
	{
		using namespace UtfImpl;

		if (!Utf16::IsValidUnicodeChar(Character.Char.Lower, Character.Char.Upper))
			return utf_cp32_t{ 0 };

		if (Character.Char.Upper)
		{

			const utf_cp16_t UpperCodepointWithoutSurrogate = static_cast<utf_cp16_t>(Character.Char.Upper - Utf16::HighSurrogateRangeStart);
			const utf_cp16_t LowerCodepointWithoutSurrogate = static_cast<utf_cp16_t>(Character.Char.Lower - Utf16::LowerSurrogateRangeStart);

			return ((static_cast<utf_cp32_t>(UpperCodepointWithoutSurrogate) << 10) | LowerCodepointWithoutSurrogate) + Utf16::SurrogatePairOffset;
		}

		return Character.Char.Lower;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 Utf32ToUtf8Bytes(const utf_char32 Character) noexcept
	{
		using namespace UtfImpl;
		using namespace UtfImpl::Utf8;

		if (!Utf32::IsValidUnicodeChar(Character.Char))
			return utf_char8{};

		utf8_bytes RetBytes;

		if (Character.Char <= Max1ByteValue)
		{
			RetBytes.Codepoints[0] = static_cast<utf_cp8_t>(Character.Char);
		}
		else if (Character.Char <= Max2ByteValue)
		{

			RetBytes.Codepoints[0] = TwoByteFlag;
			RetBytes.Codepoints[0] |= Character.Char >> NumDataBitsInFollowupByte;

			RetBytes.Codepoints[1] |= FollowupByteMask;
			RetBytes.Codepoints[1] |= Character.Char & FollowupByteDataMask;
		}
		else if (Character.Char <= Max3ByteValue)
		{

			RetBytes.Codepoints[0] = ThreeByteFlag;
			RetBytes.Codepoints[0] |= Character.Char >> (NumDataBitsInFollowupByte * 2);

			RetBytes.Codepoints[1] = FollowupByteMask;
			RetBytes.Codepoints[1] |= (Character.Char >> NumDataBitsInFollowupByte) & FollowupByteDataMask;

			RetBytes.Codepoints[2] = FollowupByteMask;
			RetBytes.Codepoints[2] |= Character.Char & FollowupByteDataMask;
		}
		else if (Character.Char <= Max4ByteValue)
		{

			RetBytes.Codepoints[0] = FourByteFlag;
			RetBytes.Codepoints[0] |= Character.Char >> (NumDataBitsInFollowupByte * 3);

			RetBytes.Codepoints[1] = FollowupByteMask;
			RetBytes.Codepoints[1] |= (Character.Char >> (NumDataBitsInFollowupByte * 2)) & FollowupByteDataMask;

			RetBytes.Codepoints[2] = FollowupByteMask;
			RetBytes.Codepoints[2] |= (Character.Char >> NumDataBitsInFollowupByte) & FollowupByteDataMask;

			RetBytes.Codepoints[3] = FollowupByteMask;
			RetBytes.Codepoints[3] |= Character.Char & FollowupByteDataMask;
		}
		else
		{

			return RetBytes;
		}

		return RetBytes;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_cp32_t Utf8BytesToUtf32(const utf_char8 Character) noexcept
	{
		using namespace UtfImpl;
		using namespace UtfImpl::Utf8;

		if ((Character[0] & 0b1000'0000) == 0)
		{
			if (!Utf8::IsValidUtf8Sequence<1>(Character[0], Character[1], Character[2], Character[3]))
				return utf_cp32_t{ 0 };

			return Character[0];
		}
		else if (Utils::IsFlagSet(Character[0], FourByteFlag))
		{
			utf_cp32_t RetChar = Utils::GetWithClearedFlag(Character[3], FollowupByteMask);
			RetChar |= Utils::GetWithClearedFlag(Character[2], FollowupByteMask) << (NumDataBitsInFollowupByte * 1);
			RetChar |= Utils::GetWithClearedFlag(Character[1], FollowupByteMask) << (NumDataBitsInFollowupByte * 2);
			RetChar |= Utils::GetWithClearedFlag(Character[0], FourByteFlag) << (NumDataBitsInFollowupByte * 3);

			if (!Utf8::IsValidUtf8Sequence<4>(Character[0], Character[1], Character[2], Character[3])
				|| !Utf32::IsValidUnicodeChar(RetChar))
				return utf_cp32_t{ 0 };

			return RetChar;
		}
		else if (Utils::IsFlagSet(Character[0], ThreeByteFlag))
		{
			utf_cp32_t RetChar = Utils::GetWithClearedFlag(Character[2], FollowupByteMask);
			RetChar |= Utils::GetWithClearedFlag(Character[1], FollowupByteMask) << (NumDataBitsInFollowupByte * 1);
			RetChar |= Utils::GetWithClearedFlag(Character[0], ThreeByteFlag) << (NumDataBitsInFollowupByte * 2);

			if (!Utf8::IsValidUtf8Sequence<3>(Character[0], Character[1], Character[2], Character[3])
				|| !Utf32::IsValidUnicodeChar(RetChar))
				return utf_cp32_t{ 0 };

			return RetChar;
		}
		else if (Utils::IsFlagSet(Character[0], TwoByteFlag))
		{
			utf_cp32_t RetChar = Utils::GetWithClearedFlag(Character[1], FollowupByteMask);
			RetChar |= Utils::GetWithClearedFlag(Character[0], TwoByteFlag) << NumDataBitsInFollowupByte;

			if (!Utf8::IsValidUtf8Sequence<2>(Character[0], Character[1], Character[2], Character[3])
				|| !Utf32::IsValidUnicodeChar(RetChar))
				return utf_cp32_t{ 0 };

			return RetChar;
		}
		else
		{

			return 0;
		}
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 Utf16PairToUtf8Bytes(const utf_char16 Character) noexcept
	{
		return Utf32ToUtf8Bytes(Utf16PairToUtf32(Character));
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 Utf8BytesToUtf16(const utf_char8 Character) noexcept
	{
		return Utf32ToUtf16Pair(Utf8BytesToUtf32(Character));
	}

	template<
		typename codepoint_iterator_type,
		typename iterator_deref_type = decltype(*std::declval<codepoint_iterator_type>()),
		typename = typename std::enable_if<sizeof(typename std::decay<iterator_deref_type>::type) == utf_char8::GetCodepointSize()>::type
	>
	class utf8_iterator : public UtfImpl::Iterator::utf_char_iterator_base<utf8_iterator<codepoint_iterator_type>, codepoint_iterator_type, utf_char8>
	{
	private:
		typedef utf8_iterator<codepoint_iterator_type> own_type;

		friend UtfImpl::Iterator::utf_char_iterator_base_child_acessor<own_type>;

	public:
		using UtfImpl::Iterator::utf_char_iterator_base<own_type, codepoint_iterator_type, utf_char8>::utf_char_iterator_base;

	public:
		utf8_iterator() = delete;

	private:
		void ReadChar()
		{
			if (this->NextCharStartIterator == this->EndIterator)
				return;

			this->CurrentChar = utf8_bytes{ 0 };

			const int CharCodepointCount = GetUtf8CharLenght(static_cast<utf_cp8_t>(*this->NextCharStartIterator));

			for (int i = 0; i < CharCodepointCount; i++)
			{

				if (this->NextCharStartIterator == this->EndIterator)
				{
					this->CurrentIterator = this->EndIterator;
					this->CurrentChar = utf8_bytes{ 0 };
					break;
				}

				this->CurrentChar[static_cast<uint8_t>(i)] = static_cast<utf_cp8_t>(*this->NextCharStartIterator);
				this->NextCharStartIterator++;
			}
		}
	};

	template<
		typename codepoint_iterator_type,
		typename iterator_deref_type = decltype(*std::declval<codepoint_iterator_type>()),
		typename = typename std::enable_if<sizeof(typename std::decay<iterator_deref_type>::type) == utf_char16::GetCodepointSize()>::type
	>
	class utf16_iterator : public UtfImpl::Iterator::utf_char_iterator_base<utf16_iterator<codepoint_iterator_type>, codepoint_iterator_type, utf_char16>
	{
	private:
		typedef utf16_iterator<codepoint_iterator_type> own_type;

		friend UtfImpl::Iterator::utf_char_iterator_base_child_acessor<own_type>;

	public:
		using UtfImpl::Iterator::utf_char_iterator_base<own_type, codepoint_iterator_type, utf_char16>::utf_char_iterator_base;

	public:
		utf16_iterator() = delete;

	private:
		UTF_CONSTEXPR void ReadChar()
		{
			if (this->NextCharStartIterator == this->EndIterator)
				return;

			this->CurrentChar = utf16_pair{ 0 };

			const int CharCodepointCount = GetUtf16CharLenght(static_cast<utf_cp16_t>(*this->NextCharStartIterator));

			if (CharCodepointCount == 0x1)
			{

				this->CurrentChar.Char.Lower = *this->NextCharStartIterator;
				this->NextCharStartIterator++;

				return;
			}

			this->CurrentChar.Char.Upper = *this->NextCharStartIterator;
			this->NextCharStartIterator++;

			if (this->NextCharStartIterator == this->EndIterator)
			{
				this->CurrentChar = utf16_pair{ 0 };
				this->CurrentIterator = this->EndIterator;
				return;
			}

			this->CurrentChar.Char.Lower = *this->NextCharStartIterator;
			this->NextCharStartIterator++;
		}
	};

	template<
		typename codepoint_iterator_type,
		typename iterator_deref_type = decltype(*std::declval<codepoint_iterator_type>()),
		typename = typename std::enable_if<sizeof(typename std::decay<iterator_deref_type>::type) == utf_char32::GetCodepointSize()>::type
	>
	class utf32_iterator : public UtfImpl::Iterator::utf_char_iterator_base<utf32_iterator<codepoint_iterator_type>, codepoint_iterator_type, utf_char32>
	{
	private:
		typedef utf32_iterator<codepoint_iterator_type> own_type;

		friend UtfImpl::Iterator::utf_char_iterator_base_child_acessor<own_type>;

	public:
		using UtfImpl::Iterator::utf_char_iterator_base<own_type, codepoint_iterator_type, utf_char32>::utf_char_iterator_base;

	public:
		utf32_iterator() = delete;

	public:
		template<typename char_type = utf_cp32_t>
		auto Replace(const char_type NewChar) -> std::enable_if_t<std::is_assignable<iterator_deref_type, char_type>::value>
		{
			this->CurrentChar = NewChar;
			*this->CurrentIterator = NewChar;
		}

	private:
		void ReadChar()
		{
			if (this->NextCharStartIterator == this->EndIterator)
				return;

			this->CurrentChar = *this->NextCharStartIterator;
			this->NextCharStartIterator++;
		}
	};

	template<typename codepoint_type,
		typename std::enable_if<sizeof(codepoint_type) == 0x1 && std::is_integral<codepoint_type>::value, int>::type = 0
	>
	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 ParseUtf8CharFromStr(const codepoint_type* Str)
	{
		if (!Str)
			return utf8_bytes{};

		const utf_cp8_t FirstCodepoint = static_cast<utf_cp8_t>(Str[0]);
		const auto CharLength = GetUtf8CharLenght(FirstCodepoint);

		if (CharLength == 0)
			return utf8_bytes{};

		utf8_bytes RetChar;
		RetChar.Codepoints[0] = FirstCodepoint;

		for (int i = 1; i < CharLength; i++)
		{
			const utf_cp8_t CurrentCodepoint = static_cast<utf_cp8_t>(Str[i]);

			if (!UtfImpl::Utf8::IsValidFollowupCodepoint(CurrentCodepoint))
				return utf8_bytes{};

			RetChar.Codepoints[i] = CurrentCodepoint;
		}

		return RetChar;
	}

	template<typename codepoint_type,
		typename std::enable_if<sizeof(codepoint_type) == 0x2 && std::is_integral<codepoint_type>::value, int>::type = 0
	>
	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 ParseUtf16CharFromStr(const codepoint_type* Str)
	{
		if (!Str)
			return utf_char16{};

		const utf_cp16_t FirstCodepoint = static_cast<utf_cp16_t>(Str[0]);

		if (GetUtf16CharLenght(FirstCodepoint) == 1)
			return utf16_pair{ FirstCodepoint };

		return utf16_pair{ FirstCodepoint,  static_cast<utf_cp16_t>(Str[1]) };
	}

	template<typename codepoint_type,
		typename std::enable_if<sizeof(codepoint_type) == 0x4 && std::is_integral<codepoint_type>::value, int>::type = 0
	>
	UTF_CONSTEXPR UTF_NODISCARD
		utf_char32 ParseUtf32CharFromStr(const codepoint_type* Str)
	{
		if (!Str)
			return utf_char32{};

		return static_cast<utf_cp32_t>(Str[0]);
	}

	template<typename utf8_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf8_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf16StringToUtf8String(utf16_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf8_char_string RetString;

		for (const utf_char16 Char : StringIteratorToConvert)
		{
			const auto NewChar = Utf16PairToUtf8Bytes(Char);

			for (int i = 0; i < NewChar.GetNumCodepoints(); i++)
				RetString += static_cast<target_char_type>(NewChar[static_cast<uint8_t>(i)]);
		}

		return RetString;
	}

	template<typename utf8_char_string, typename utf16_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<const utf16_char_string>())),
		typename = utf16_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf16StringToUtf8String(const utf16_char_string& StringToConvert)
	{
		return Utf16StringToUtf8String<utf8_char_string>(utf16_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf8_char_string, typename utf16_char_type, size_t CStrLenght,
		typename = utf16_iterator<utf16_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf16StringToUtf8String(utf16_char_type(&StringToConvert)[CStrLenght])
	{
		return Utf16StringToUtf8String<utf8_char_string>(utf16_iterator<const utf16_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf8_char_string, typename utf16_char_type,
		typename = utf16_iterator<utf16_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf16StringToUtf8String(const utf16_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf16StringToUtf8String<utf8_char_string>(utf16_iterator<const utf16_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename utf8_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf8_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf32StringToUtf8String(utf32_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf8_char_string RetString;

		for (const utf_char32 Char : StringIteratorToConvert)
		{
			const auto NewChar = Utf32ToUtf8Bytes(Char);

			for (int i = 0; i < NewChar.GetNumCodepoints(); i++)
				RetString += static_cast<target_char_type>(NewChar[static_cast<uint8_t>(i)]);
		}

		return RetString;
	}

	template<typename utf8_char_string, typename utf32_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<utf32_char_string>())),
		typename = utf32_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf32StringToUtf8String(const utf32_char_string& StringToConvert)
	{
		return Utf32StringToUtf8String<utf8_char_string>(utf32_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf8_char_string, typename utf32_char_type, size_t cstr_lenght,
		typename = utf32_iterator<utf32_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf32StringToUtf8String(utf32_char_type(&StringToConvert)[cstr_lenght])
	{
		return Utf32StringToUtf8String<utf8_char_string>(utf32_iterator<const utf32_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf8_char_string, typename utf32_char_type,
		typename = utf32_iterator<utf32_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf8_char_string Utf32StringToUtf8String(const utf32_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf32StringToUtf8String<utf8_char_string>(utf32_iterator<const utf32_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename utf16_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf16_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf8StringToUtf16String(utf8_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf16_char_string RetString;

		for (const utf_char8 Char : StringIteratorToConvert)
		{
			const auto NewChar = Utf8BytesToUtf16(Char);

			if (NewChar.GetNumCodepoints() > 1)
				RetString += NewChar.Get().Upper;

			RetString += NewChar.Get().Lower;
		}

		return RetString;
	}

	template<typename utf16_char_string, typename utf8_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<utf8_char_string>())),
		typename = utf8_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf8StringToUtf16String(const utf8_char_string& StringToConvert)
	{
		return Utf8StringToUtf16String<utf16_char_string>(utf8_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf16_char_string, typename utf8_char_type, size_t cstr_lenght,
		typename = utf8_iterator<utf8_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf8StringToUtf16String(utf8_char_type(&StringToConvert)[cstr_lenght])
	{
		return Utf32StringToUtf16String<utf16_char_string>(utf8_iterator<const utf8_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf16_char_string, typename utf8_char_type,
		typename = utf8_iterator<utf8_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf8StringToUtf16String(const utf8_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf32StringToUtf16String<utf16_char_string>(utf32_iterator<const utf8_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename utf16_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf16_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf32StringToUtf16String(utf32_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf16_char_string RetString;

		for (const utf_char32 Char : StringIteratorToConvert)
		{
			const auto NewChar = Utf32ToUtf16Pair(Char);

			if (NewChar.GetNumCodepoints() > 1)
				RetString += NewChar.Get().Upper;

			RetString += NewChar.Get().Lower;
		}

		return RetString;
	}

	template<typename utf16_char_string, typename utf32_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<utf32_char_string>())),
		typename = utf32_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf32StringToUtf16String(const utf32_char_string& StringToConvert)
	{
		return Utf32StringToUtf16String<utf16_char_string>(utf32_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf16_char_string, typename utf32_char_type, size_t cstr_lenght,
		typename = utf32_iterator<utf32_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf32StringToUtf16String(utf32_char_type(&StringToConvert)[cstr_lenght])
	{
		return Utf32StringToUtf16String<utf16_char_string>(utf32_iterator<const utf32_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf16_char_string, typename utf32_char_type,
		typename = utf8_iterator<utf32_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf16_char_string Utf32StringToUtf16String(const utf32_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf32StringToUtf16String<utf16_char_string>(utf32_iterator<const utf32_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename utf32_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf32_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf8StringToUtf32String(utf8_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf32_char_string RetString;

		for (const utf_char8 Char : StringIteratorToConvert)
		{
			RetString += Utf8BytesToUtf32(Char);
		}

		return RetString;
	}

	template<typename utf32_char_string, typename utf8_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<utf8_char_string>())),
		typename = utf8_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf8StringToUtf32String(const utf8_char_string& StringToConvert)
	{
		return Utf8StringToUtf32String<utf32_char_string>(utf8_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf32_char_string, typename utf8_char_type, size_t cstr_lenght,
		typename = utf8_iterator<utf8_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf8StringToUtf32String(utf8_char_type(&StringToConvert)[cstr_lenght])
	{
		return Utf8StringToUtf32String<utf32_char_string>(utf8_iterator<const utf8_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf32_char_string, typename utf8_char_type, size_t cstr_lenght,
		typename = utf8_iterator<utf8_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf8StringToUtf32String(const utf8_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf8StringToUtf32String<utf32_char_string>(utf8_iterator<const utf8_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename utf32_char_string,
		typename inner_iterator,
		typename target_char_type = typename std::decay<decltype(*std::begin(std::declval<utf32_char_string>()))>::type
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf16StringToUtf32String(utf16_iterator<inner_iterator> StringIteratorToConvert)
	{
		utf32_char_string RetString;

		for (const utf_char16 Char : StringIteratorToConvert)
		{
			RetString += Utf16PairToUtf32(Char).Get();
		}

		return RetString;
	}

	template<typename utf32_char_string, typename utf16_char_string,
		typename inner_iterator = decltype(std::begin(std::declval<const utf16_char_string>())),
		typename = utf16_iterator<inner_iterator>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf16StringToUtf32String(const utf16_char_string& StringToConvert)
	{
		return Utf16StringToUtf32String<utf32_char_string>(utf16_iterator<inner_iterator>(StringToConvert));
	}

	template<typename utf32_char_string, typename utf16_char_type, size_t cstr_lenght,
		typename = utf16_iterator<utf16_char_type*>
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf16StringToUtf32String(utf16_char_type(&StringToConvert)[cstr_lenght])
	{
		return Utf16StringToUtf32String<utf32_char_string>(utf16_iterator<const utf16_char_type*>(std::begin(StringToConvert), std::end(StringToConvert)));
	}

	template<typename utf32_char_string, typename utf16_char_type, size_t cstr_lenght,
		typename = utf16_iterator<utf16_char_type*>
	>
		UTF_CONSTEXPR20 UTF_NODISCARD
		utf32_char_string Utf16StringToUtf32String(const utf16_char_type* StringToConvert, int NonNullTermiantedLength)
	{
		return Utf16StringToUtf32String<utf32_char_string>(utf16_iterator<const utf16_char_type*>(StringToConvert, StringToConvert + NonNullTermiantedLength));
	}

	template<typename wstring_type = std::wstring, typename string_type = std::string,
		typename = decltype(std::begin(std::declval<wstring_type>())),
		typename = decltype(std::end(std::declval<wstring_type>()))
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		string_type WStringToString(const wstring_type& WideString)
	{
		using char_type = typename std::decay<decltype(*std::begin(std::declval<wstring_type>()))>::type;

		struct dummy_2byte_str { uint16_t* begin() const { return nullptr; };   uint16_t* end() const { return nullptr; }; };
		struct dummy_4byte_str { uint32_t* begin() const { return nullptr; };   uint32_t* end() const { return nullptr; }; };

		if UTF_IF_CONSTEXPR (sizeof(char_type) == 0x2)
		{
			using type_to_use = typename std::conditional<sizeof(char_type) == 2, wstring_type, dummy_2byte_str>::type;
			return Utf16StringToUtf8String<string_type, type_to_use>(UtfImpl::Utils::ForceCastIfMissmatch<const type_to_use&, const wstring_type&>(WideString));
		}
		else
		{
			using type_to_use = typename std::conditional<sizeof(char_type) == 4, wstring_type, dummy_4byte_str>::type;
			return Utf32StringToUtf8String<string_type, type_to_use>(UtfImpl::Utils::ForceCastIfMissmatch<const type_to_use&, const wstring_type&>(WideString));
		}
	}

	template<typename string_type = std::string, typename wstring_type = std::wstring,
		typename = decltype(std::begin(std::declval<string_type>())),
		typename = decltype(std::end(std::declval<string_type>()))
	>
	UTF_CONSTEXPR20 UTF_NODISCARD
		wstring_type StringToWString(const string_type& NarrowString)
	{
		using char_type = typename std::decay<decltype(*std::begin(std::declval<wstring_type>()))>::type;

		struct dummy_2byte_str { uint16_t* begin() const { return nullptr; };   uint16_t* end() const { return nullptr; }; };
		struct dummy_4byte_str { uint32_t* begin() const { return nullptr; };   uint32_t* end() const { return nullptr; }; };

		if UTF_IF_CONSTEXPR(sizeof(char_type) == 0x2)
		{
			using type_to_use = typename std::conditional<sizeof(char_type) == 2, wstring_type, dummy_2byte_str>::type;
			return Utf8StringToUtf16String<type_to_use, string_type>(NarrowString);
		}
		else
		{
			using type_to_use = typename std::conditional<sizeof(char_type) == 4, wstring_type, dummy_4byte_str>::type;
			return Utf8StringToUtf32String<type_to_use, string_type>(NarrowString);
		}
	}

	template<typename byte_iterator_type>
	UTF_CONSTEXPR byte_iterator_type ReplaceUtf8(byte_iterator_type Begin, byte_iterator_type End, utf_cp8_t CharToReplace, utf_cp8_t ReplacementChar)
	{
		using namespace UtfImpl;

		if (Begin == End)
			return End;

		const auto ToReplaceSize = GetUtf8CharLenght(CharToReplace);
		const auto ReplacementSize = GetUtf8CharLenght(ReplacementChar);

		if (ToReplaceSize == ReplacementSize)
		{

		}
		else if (ToReplaceSize < ReplacementSize)
		{

		}
		else
		{

		}
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf8>::utf_char(utf8_bytes InChar) noexcept
		: Char(InChar)
	{
	}

	template<typename char_type, typename>
	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf8>::utf_char(const char_type* SingleCharString) noexcept
		: utf_char<UtfEncodingType::Utf8>(ParseUtf8CharFromStr(SingleCharString))
	{
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf8>& utf_char<UtfEncodingType::Utf8>::operator=(utf8_bytes inBytse) noexcept
	{
		Char.Codepoints[0] = inBytse.Codepoints[0];
		Char.Codepoints[1] = inBytse.Codepoints[1];
		Char.Codepoints[2] = inBytse.Codepoints[2];
		Char.Codepoints[3] = inBytse.Codepoints[3];

		return *this;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf8>::operator==(utf_char8 Other) const noexcept
	{
		return Char == Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_cp8_t& utf_char<UtfEncodingType::Utf8>::operator[](const uint8_t Index)
	{
#ifdef _DEBUG
		if (Index >= 0x4)
			throw std::out_of_range("Index was greater than 4!");
#endif
		return Char.Codepoints[Index];
	}

	UTF_CONSTEXPR UTF_NODISCARD
		const utf_cp8_t& utf_char<UtfEncodingType::Utf8>::operator[](const uint8_t Index) const
	{
#ifdef _DEBUG
		if (Index >= 0x4)
			throw std::out_of_range("Index was greater than 4!");
#endif
		return Char.Codepoints[Index];
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf8>::operator!=(utf_char8 Other) const noexcept
	{
		return Char != Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 utf_char<UtfEncodingType::Utf8>::GetAsUtf8() const noexcept
	{
		return *this;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 utf_char<UtfEncodingType::Utf8>::GetAsUtf16() const noexcept
	{
		return Utf8BytesToUtf16(*this);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char32 utf_char<UtfEncodingType::Utf8>::GetAsUtf32() const noexcept
	{
		return Utf8BytesToUtf32(*this);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf8_bytes utf_char<UtfEncodingType::Utf8>::Get() const
	{
		return Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		UtfEncodingType utf_char<UtfEncodingType::Utf8>::GetEncoding() const noexcept
	{
		return UtfEncodingType::Utf8;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		uint8_t utf_char<UtfEncodingType::Utf8>::GetNumCodepoints() const noexcept
	{
		return GetUtf8CharLenght(Char.Codepoints[0]);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		 uint8_t utf_char<UtfEncodingType::Utf8>::GetCodepointSize() noexcept
	{
		return 0x1;
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf16>::utf_char(utf16_pair InChar) noexcept
		: Char(InChar)
	{
	}

	template<typename char_type, typename>
	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf16>::utf_char(const char_type* SingleCharString) noexcept
		: utf_char<UtfEncodingType::Utf16>(ParseUtf16CharFromStr(SingleCharString))
	{
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf16>& utf_char<UtfEncodingType::Utf16>::operator=(utf16_pair inBytse) noexcept
	{
		Char.Upper = inBytse.Upper;
		Char.Lower = inBytse.Lower;

		return *this;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf16>::operator==(utf_char16 Other) const noexcept
	{
		return Char == Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf16>::operator!=(utf_char16 Other) const noexcept
	{
		return Char != Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 utf_char<UtfEncodingType::Utf16>::GetAsUtf8() const noexcept
	{
		return Utf16PairToUtf8Bytes(Char);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 utf_char<UtfEncodingType::Utf16>::GetAsUtf16() const noexcept
	{
		return Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char32 utf_char<UtfEncodingType::Utf16>::GetAsUtf32() const noexcept
	{
		return Utf16PairToUtf32(Char);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf16_pair utf_char<UtfEncodingType::Utf16>::Get() const noexcept
	{
		return Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		UtfEncodingType utf_char<UtfEncodingType::Utf16>::GetEncoding() const noexcept
	{
		return UtfEncodingType::Utf16;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		uint8_t utf_char<UtfEncodingType::Utf16>::GetNumCodepoints() const noexcept
	{
		return GetUtf16CharLenght(Char.Upper);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		 uint8_t utf_char<UtfEncodingType::Utf16>::GetCodepointSize() noexcept
	{
		return 0x2;
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf32>::utf_char(utf_cp32_t InChar) noexcept
		: Char(InChar)
	{
	}

	template<typename char_type, typename>
	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf32>::utf_char(const char_type* SingleCharString) noexcept
		: Char(*SingleCharString)
	{
	}

	UTF_CONSTEXPR utf_char<UtfEncodingType::Utf32>& utf_char<UtfEncodingType::Utf32>::operator=(utf_cp32_t inBytse) noexcept
	{
		Char = inBytse;
		return *this;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf32>::operator==(utf_char32 Other) const noexcept
	{
		return Char == Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		bool utf_char<UtfEncodingType::Utf32>::operator!=(utf_char32 Other) const noexcept
	{
		return Char != Other.Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char8 utf_char<UtfEncodingType::Utf32>::GetAsUtf8() const noexcept
	{
		return Utf32ToUtf8Bytes(Char);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char16 utf_char<UtfEncodingType::Utf32>::GetAsUtf16() const noexcept
	{
		return Utf32ToUtf16Pair(Char);
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_char32 utf_char<UtfEncodingType::Utf32>::GetAsUtf32() const noexcept
	{
		return Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		utf_cp32_t utf_char<UtfEncodingType::Utf32>::Get() const noexcept
	{
		return Char;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		UtfEncodingType utf_char<UtfEncodingType::Utf32>::GetEncoding() const noexcept
	{
		return UtfEncodingType::Utf32;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		uint8_t utf_char<UtfEncodingType::Utf32>::GetNumCodepoints() const noexcept
	{
		return 0x1;
	}

	UTF_CONSTEXPR UTF_NODISCARD
		 uint8_t utf_char<UtfEncodingType::Utf32>::GetCodepointSize() noexcept
	{
		return 0x4;
	}
}

#undef UTF_CONSTEXPR
#undef UTF_CONSTEXPR14
#undef UTF_CONSTEXPR17
#undef UTF_CONSTEXPR20
#undef UTF_CONSTEXPR23
#undef UTF_CONSTEXPR26

#if (defined(_MSC_VER))
#pragma warning (pop)
#elif (defined(__CLANG__) || defined(__GNUC__))
#pragma GCC diagnostic pop
#endif
}

std::string GetUnrealContainersHeaderContent() {
    return R"AVALONIUM_UC(#pragma once

#include <string>
#include <stdexcept>
#include <iostream>
#include <optional>
#include "UtfN.hpp"

#ifndef IMPORT_CPP_SDK_INTO_IDA
namespace UC
{
#endif

	typedef int8_t  int8;
	typedef int16_t int16;
	typedef int32_t int32;
	typedef int64_t int64;

	typedef uint8_t  uint8;
	typedef uint16_t uint16;
	typedef uint32_t uint32;
	typedef uint64_t uint64;

	template<typename ArrayElementType>
	class TArray;

	template<typename SparseArrayElementType>
	class TSparseArray;

	template<typename SetElementType>
	class TSet;

	template<typename KeyElementType, typename ValueElementType>
	class TMap;

	template<typename KeyElementType, typename ValueElementType>
	class TPair;

	namespace Iterators
	{
		class FSetBitIterator;

		template<typename ArrayType>
		class TArrayIterator;

		template<class ContainerType>
		class TContainerIterator;

		template<typename SparseArrayElementType>
		using TSparseArrayIterator = TContainerIterator<TSparseArray<SparseArrayElementType>>;

		template<typename SetElementType>
		using TSetIterator = TContainerIterator<TSet<SetElementType>>;

		template<typename KeyElementType, typename ValueElementType>
		using TMapIterator = TContainerIterator<TMap<KeyElementType, ValueElementType>>;
	}

	namespace ContainerImpl
	{
		namespace HelperFunctions
		{
			inline uint32 FloorLog2(uint32 Value)
			{
				uint32 pos = 0;
				if (Value >= 1 << 16) { Value >>= 16; pos += 16; }
				if (Value >= 1 << 8) { Value >>= 8; pos += 8; }
				if (Value >= 1 << 4) { Value >>= 4; pos += 4; }
				if (Value >= 1 << 2) { Value >>= 2; pos += 2; }
				if (Value >= 1 << 1) { pos += 1; }
				return pos;
			}

			inline uint32 CountLeadingZeros(uint32 Value)
			{
				if (Value == 0)
					return 32;

				return 31 - FloorLog2(Value);
			}
		}

		template<int32 Size, uint32 Alignment>
		struct TAlignedBytes
		{
			uint8 Pad[Size];
		};

		template<uint32 NumInlineElements>
		class TInlineAllocator
		{
		public:
			template<typename ElementType>
			class ForElementType
			{
			private:
				static constexpr int32 ElementSize = sizeof(ElementType);
				static constexpr int32 ElementAlign = alignof(ElementType);

				static constexpr int32 InlineDataSizeBytes = NumInlineElements * ElementSize;

			private:
				TAlignedBytes<ElementSize, ElementAlign> InlineData[NumInlineElements];
				ElementType* SecondaryData;

			public:
				ForElementType()
					: InlineData{ 0x0 }, SecondaryData(nullptr)
				{
				}

				ForElementType(ForElementType&&) = default;
				ForElementType(const ForElementType&) = default;

			public:
				ForElementType& operator=(ForElementType&&) = default;
				ForElementType& operator=(const ForElementType&) = default;

			public:
				inline const ElementType* GetAllocation() const { return SecondaryData ? SecondaryData : reinterpret_cast<const ElementType*>(&InlineData); }

				inline uint32 GetNumInlineBytes() const { return NumInlineElements; }
			};
		};

		class FBitArray
		{
		protected:
			static constexpr int32 NumBitsPerDWORD = 32;
			static constexpr int32 NumBitsPerDWORDLogTwo = 5;

		private:
			TInlineAllocator<4>::ForElementType<int32> Data;
			int32 NumBits;
			int32 MaxBits;

		public:
			FBitArray()
				: NumBits(0), MaxBits(Data.GetNumInlineBytes() * NumBitsPerDWORD)
			{
			}

			FBitArray(const FBitArray&) = default;

			FBitArray(FBitArray&&) = default;

		public:
			FBitArray& operator=(FBitArray&&) = default;

			FBitArray& operator=(const FBitArray& Other) = default;

		private:
			inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }

		public:
			inline int32 Num() const { return NumBits; }
			inline int32 Max() const { return MaxBits; }

			inline const uint32* GetData() const { return reinterpret_cast<const uint32*>(Data.GetAllocation()); }

			inline bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < NumBits; }

			inline bool IsValid() const { return GetData() && NumBits > 0; }

		public:
			inline bool operator[](int32 Index) const { VerifyIndex(Index); return GetData()[Index / NumBitsPerDWORD] & (1 << (Index & (NumBitsPerDWORD - 1))); }

			inline bool operator==(const FBitArray& Other) const { return NumBits == Other.NumBits && GetData() == Other.GetData(); }
			inline bool operator!=(const FBitArray& Other) const { return NumBits != Other.NumBits || GetData() != Other.GetData(); }

		public:
			friend Iterators::FSetBitIterator begin(const FBitArray& Array);
			friend Iterators::FSetBitIterator end  (const FBitArray& Array);
		};

		template<typename SparseArrayType>
		union TSparseArrayElementOrFreeListLink
		{
			SparseArrayType ElementData;

			struct
			{
				int32 PrevFreeIndex;
				int32 NextFreeIndex;
			};
		};

		template<typename SetType>
		class SetElement
		{
		public:
			friend class TSet<SetType>;

		private:
			SetType Value;
			int32 HashNextId;
			int32 HashIndex;
		};
	}

	template <typename KeyType, typename ValueType>
	class TPair
	{
	public:
		KeyType First;
		ValueType Second;

	public:
		TPair(KeyType Key, ValueType Value)
			: First(Key), Second(Value)
		{
		}

	public:
		inline       KeyType& Key()       { return First; }
		inline const KeyType& Key() const { return First; }

		inline       ValueType& Value()       { return Second; }
		inline const ValueType& Value() const { return Second; }
	};

	template<typename ArrayElementType>
	class TArray
	{
	private:
		template<typename ArrayElementType>
		friend class TAllocatedArray;

		template<typename SparseArrayElementType>
		friend class TSparseArray;

	protected:
		static constexpr uint64 ElementAlign = alignof(ArrayElementType);
		static constexpr uint64 ElementSize = sizeof(ArrayElementType);

	protected:
		ArrayElementType* Data;
		int32 NumElements;
		int32 MaxElements;

	public:
		TArray()
			: TArray(nullptr, 0, 0)
		{
		}

		TArray(ArrayElementType* Data, int32 NumElements, int32 MaxElements)
			: Data(Data), NumElements(NumElements), MaxElements(MaxElements)
		{
		}

		TArray(const TArray&) = default;

		TArray(TArray&&) = default;

	public:
		TArray& operator=(TArray&&) = default;
		TArray& operator=(const TArray&) = default;

	private:
		inline int32 GetSlack() const { return MaxElements - NumElements; }

		inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }

		inline       ArrayElementType& GetUnsafe(int32 Index)       { return Data[Index]; }
		inline const ArrayElementType& GetUnsafe(int32 Index) const { return Data[Index]; }

	public:

		inline bool Add(const ArrayElementType& Element)
		{
			if (GetSlack() <= 0)
				return false;

			Data[NumElements] = Element;
			NumElements++;

			return true;
		}

		inline bool Remove(int32 Index)
		{
			if (!IsValidIndex(Index))
				return false;

			NumElements--;

			for (int i = Index; i < NumElements; i++)
			{

				Data[i] = Data[i + 1];
			}

			return true;
		}

		inline void Clear()
		{
			NumElements = 0;

			if (Data)
				memset(Data, 0, NumElements * ElementSize);
		}

    public:
        template<typename OtherType>
        inline std::optional<ArrayElementType> Find(const OtherType& ElementToSearch, bool(*IsEqual)(const ArrayElementType&, const OtherType&)) const
        {
            for (const auto& Element : *this)
            {
                if (IsEqual(Element, ElementToSearch))
                    return Element;
            }

            return {};
        }

        inline std::optional<ArrayElementType> Find(const ArrayElementType& ElementToSearch) const
            requires std::equality_comparable<ArrayElementType>
        {
            for (const auto& Element : *this)
            {
                if (Element == ElementToSearch)
                    return Element;
            }

            return {};
        }

        template<typename OtherType>
        inline bool Contains(const OtherType& ElementToSearch,     bool(*IsEqual)(const ArrayElementType&, const OtherType&)) const
        {
            return Find<OtherType>(ElementToSearch, IsEqual).has_value();
        }

        inline bool Contains(const ArrayElementType& ElementToSearch) const
            requires std::equality_comparable<ArrayElementType>
        {
            return Find(ElementToSearch).has_value();
        }

	public:
		inline int32 Num() const { return NumElements; }
		inline int32 Max() const { return MaxElements; }

		inline const ArrayElementType* GetDataPtr() const { return Data; }

		inline bool IsValidIndex(int32 Index) const { return Data && Index >= 0 && Index < NumElements; }

		inline bool IsValid() const { return Data && NumElements > 0 && MaxElements >= NumElements; }

	public:
		inline       ArrayElementType& operator[](int32 Index)       { VerifyIndex(Index); return Data[Index]; }
		inline const ArrayElementType& operator[](int32 Index) const { VerifyIndex(Index); return Data[Index]; }

		inline bool operator==(const TArray<ArrayElementType>& Other) const { return Data == Other.Data; }
		inline bool operator!=(const TArray<ArrayElementType>& Other) const { return Data != Other.Data; }

		inline explicit operator bool() const { return IsValid(); };

	public:
		template<typename T> friend Iterators::TArrayIterator<T> begin(const TArray& Array);
		template<typename T> friend Iterators::TArrayIterator<T> end  (const TArray& Array);
	};

	class FString : public TArray<wchar_t>
	{
	public:
		friend std::ostream& operator<<(std::ostream& Stream, const FString& Str) { return Stream << Str.ToString(); }

	public:
		using TArray::TArray;

		FString(const wchar_t* Str)
		{
			const uint32 NullTerminatedLength = static_cast<uint32>(wcslen(Str) + 0x1);

			Data = const_cast<wchar_t*>(Str);
			NumElements = NullTerminatedLength;
			MaxElements = NullTerminatedLength;
		}

		FString(wchar_t* Str, int32 Num, int32 Max)
		{
			Data = Str;
			NumElements = Num;
			MaxElements = Max;
		}

	public:
		inline std::string ToString() const
		{
			if (*this)
			{
				return UtfN::Utf16StringToUtf8String<std::string>(Data, NumElements  - 1);
			}

			return "";
		}

		inline std::wstring ToWString() const
		{
			if (*this)
				return std::wstring(Data);

			return L"";
		}

	public:
		inline       wchar_t* CStr()       { return Data; }
		inline const wchar_t* CStr() const { return Data; }

	public:
		inline bool operator==(const FString& Other) const { return Other ? NumElements == Other.NumElements && wcscmp(Data, Other.Data) == 0 : false; }
		inline bool operator!=(const FString& Other) const { return Other ? NumElements != Other.NumElements || wcscmp(Data, Other.Data) != 0 : true; }
	};

	class FUtf8String : public TArray<char8_t>
	{
	public:
		friend std::ostream& operator<<(std::ostream& Stream, const FUtf8String& Str) { return Stream << Str.ToString(); }

	private:
		inline const char* GetDataAsConstCharPtr() const
		{
			return reinterpret_cast<const char*>(Data);
		}

	public:
		using TArray::TArray;

		FUtf8String(const char8_t* Str)
		{
			Data = const_cast<char8_t*>(Str);

			const uint32 NullTerminatedLength = static_cast<uint32>(strlen(GetDataAsConstCharPtr()) + 0x1);

			NumElements = NullTerminatedLength;
			MaxElements = NullTerminatedLength;
		}

		FUtf8String(char8_t* Str, int32 Num, int32 Max)
		{
			Data = Str;
			NumElements = Num;
			MaxElements = Max;
		}

	public:
		inline std::string ToString() const
		{
			if (*this)
			{
				return std::string(GetDataAsConstCharPtr(), NumElements - 1);
			}

			return "";
		}

		inline std::wstring ToWString() const
		{
			if (*this)
				return UtfN::StringToWString<std::string>(ToString());

			return L"";
		}

	public:
		inline       char8_t* CStr()       { return Data; }
		inline const char8_t* CStr() const { return Data; }

	public:
		inline bool operator==(const FUtf8String& Other) const { return Other ? NumElements == Other.NumElements && strcmp(GetDataAsConstCharPtr(), Other.GetDataAsConstCharPtr()) == 0 : false; }
		inline bool operator!=(const FUtf8String& Other) const { return Other ? NumElements != Other.NumElements || strcmp(GetDataAsConstCharPtr(), Other.GetDataAsConstCharPtr()) != 0 : true; }
	};

	class FAnsiString : public TArray<char>
	{
	public:
		friend std::ostream& operator<<(std::ostream& Stream, const FAnsiString& Str) { return Stream << Str.ToString(); }

	public:
		using TArray::TArray;

		FAnsiString(const char* Str)
		{
			const uint32 NullTerminatedLength = static_cast<uint32>(strlen(Str) + 0x1);

			Data = const_cast<char*>(Str);
			NumElements = NullTerminatedLength;
			MaxElements = NullTerminatedLength;
		}

		FAnsiString(char* Str, int32 Num, int32 Max)
		{
			Data = Str;
			NumElements = Num;
			MaxElements = Max;
		}

	public:
		inline std::string ToString() const
		{
			if (*this)
			{
				return std::string(Data, NumElements - 1);
			}

			return "";
		}

		inline std::wstring ToWString() const
		{
			if (*this)
				return UtfN::StringToWString<std::string>(ToString());

			return L"";
		}

	public:
		inline       char* CStr() { return Data; }
		inline const char* CStr() const { return Data; }

	public:
		inline bool operator==(const FAnsiString& Other) const { return Other ? NumElements == Other.NumElements && strcmp(Data, Other.Data) == 0 : false; }
		inline bool operator!=(const FAnsiString& Other) const { return Other ? NumElements != Other.NumElements || strcmp(Data, Other.Data) != 0 : true; }
	};

	template<typename ArrayElementType>
	class TAllocatedArray : public TArray<ArrayElementType>
	{
	public:
		TAllocatedArray() = delete;

	public:
		TAllocatedArray(int32 Size)
		{
			this->Data = static_cast<ArrayElementType*>(malloc(Size * sizeof(ArrayElementType)));
			this->NumElements = 0x0;
			this->MaxElements = Size;
		}

		~TAllocatedArray()
		{
			if (this->Data)
				free(this->Data);

			this->NumElements = 0x0;
			this->MaxElements = 0x0;
		}

	public:
		inline operator       TArray<ArrayElementType>()       { return *reinterpret_cast<      TArray<ArrayElementType>*>(this); }
		inline operator const TArray<ArrayElementType>() const { return *reinterpret_cast<const TArray<ArrayElementType>*>(this); }
	};

	class FAllocatedString : public FString
	{
	public:
		FAllocatedString() = delete;

	public:
		FAllocatedString(int32 Size)
		{
			Data = static_cast<wchar_t*>(malloc(Size * sizeof(wchar_t)));
			NumElements = 0x0;
			MaxElements = Size;
		}

		~FAllocatedString()
		{
			if (Data)
				free(Data);

			NumElements = 0x0;
			MaxElements = 0x0;
		}

	public:
		inline operator       FString()       { return *reinterpret_cast<      FString*>(this); }
		inline operator const FString() const { return *reinterpret_cast<const FString*>(this); }
	};
	template<typename SparseArrayElementType>
	class TSparseArray
	{
	private:
		static constexpr uint32 ElementAlign = alignof(SparseArrayElementType);
		static constexpr uint32 ElementSize = sizeof(SparseArrayElementType);

	private:
		using FElementOrFreeListLink = ContainerImpl::TSparseArrayElementOrFreeListLink<ContainerImpl::TAlignedBytes<ElementSize, ElementAlign>>;

	private:
		TArray<FElementOrFreeListLink> Data;
		ContainerImpl::FBitArray AllocationFlags;
		int32 FirstFreeIndex;
		int32 NumFreeIndices;

	public:
		TSparseArray()
			: FirstFreeIndex(-1), NumFreeIndices(0)
		{
		}

		TSparseArray(TSparseArray&&) = default;
		TSparseArray(const TSparseArray&) = default;

	public:
		TSparseArray& operator=(TSparseArray&&) = default;
		TSparseArray& operator=(const TSparseArray&) = default;

	private:
		inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }

	public:
		inline int32 NumAllocated() const { return Data.Num(); }

		inline int32 Num() const { return NumAllocated() - NumFreeIndices; }
		inline int32 Max() const { return Data.Max(); }

		inline bool IsValidIndex(int32 Index) const { return Data.IsValidIndex(Index) && AllocationFlags[Index]; }

		inline bool IsValid() const { return Data.IsValid() && AllocationFlags.IsValid(); }

	public:
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return AllocationFlags; }

	public:
		inline       SparseArrayElementType& operator[](int32 Index)       { VerifyIndex(Index); return *reinterpret_cast<SparseArrayElementType*>(&Data.GetUnsafe(Index).ElementData); }
		inline const SparseArrayElementType& operator[](int32 Index) const { VerifyIndex(Index); return *reinterpret_cast<const SparseArrayElementType*>(&Data.GetUnsafe(Index).ElementData); }

		inline bool operator==(const TSparseArray<SparseArrayElementType>& Other) const { return Data == Other.Data; }
		inline bool operator!=(const TSparseArray<SparseArrayElementType>& Other) const { return Data != Other.Data; }

	public:
		template<typename T> friend Iterators::TSparseArrayIterator<T> begin(const TSparseArray& Array);
		template<typename T> friend Iterators::TSparseArrayIterator<T> end  (const TSparseArray& Array);
	};

	template<typename SetElementType>
	class TSet
	{
	private:
		static constexpr uint32 ElementAlign = alignof(SetElementType);
		static constexpr uint32 ElementSize = sizeof(SetElementType);

	private:
		using SetDataType = ContainerImpl::SetElement<SetElementType>;
		using HashType = ContainerImpl::TInlineAllocator<1>::ForElementType<int32>;

	private:
		TSparseArray<SetDataType> Elements;
		HashType Hash;
		int32 HashSize;

	public:
		TSet()
			: HashSize(0)
		{
		}

		TSet(TSet&&) = default;
		TSet(const TSet&) = default;

	public:
		TSet& operator=(TSet&&) = default;
		TSet& operator=(const TSet&) = default;

	private:
		inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }

	public:
		inline int32 NumAllocated() const { return Elements.NumAllocated(); }

		inline int32 Num() const { return Elements.Num(); }
		inline int32 Max() const { return Elements.Max(); }

		inline bool IsValidIndex(int32 Index) const { return Elements.IsValidIndex(Index); }

		inline bool IsValid() const { return Elements.IsValid(); }

	public:
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return Elements.GetAllocationFlags(); }

	public:
		inline       SetElementType& operator[] (int32 Index)       { return Elements[Index].Value; }
		inline const SetElementType& operator[] (int32 Index) const { return Elements[Index].Value; }

		inline bool operator==(const TSet<SetElementType>& Other) const { return Elements == Other.Elements; }
		inline bool operator!=(const TSet<SetElementType>& Other) const { return Elements != Other.Elements; }

	public:
		template<typename T> friend Iterators::TSetIterator<T> begin(const TSet& Set);
		template<typename T> friend Iterators::TSetIterator<T> end  (const TSet& Set);
	};

	template<typename KeyElementType, typename ValueElementType>
	class TMap
	{
	public:
		using ElementType = TPair<KeyElementType, ValueElementType>;

	private:
		TSet<ElementType> Elements;

	private:
		inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }

	public:
		inline int32 NumAllocated() const { return Elements.NumAllocated(); }

		inline int32 Num() const { return Elements.Num(); }
		inline int32 Max() const { return Elements.Max(); }

		inline bool IsValidIndex(int32 Index) const { return Elements.IsValidIndex(Index); }

		inline bool IsValid() const { return Elements.IsValid(); }

	public:
		const ContainerImpl::FBitArray& GetAllocationFlags() const { return Elements.GetAllocationFlags(); }

	public:
		inline decltype(auto) Find(const KeyElementType& Key, bool(*Equals)(const KeyElementType& LeftKey, const KeyElementType& RightKey))
		{
			for (auto It = begin(*this); It != end(*this); ++It)
			{
				if (Equals(It->Key(), Key))
					return It;
			}

			return end(*this);
		}

	public:
		inline       ElementType& operator[] (int32 Index)       { return Elements[Index]; }
		inline const ElementType& operator[] (int32 Index) const { return Elements[Index]; }

		inline bool operator==(const TMap<KeyElementType, ValueElementType>& Other) const { return Elements == Other.Elements; }
		inline bool operator!=(const TMap<KeyElementType, ValueElementType>& Other) const { return Elements != Other.Elements; }

	public:
		template<typename KeyType, typename ValueType> friend Iterators::TMapIterator<KeyType, ValueType> begin(const TMap& Map);
		template<typename KeyType, typename ValueType> friend Iterators::TMapIterator<KeyType, ValueType> end  (const TMap& Map);
	};

	namespace Iterators
	{
		class FRelativeBitReference
		{
		protected:
			static constexpr int32 NumBitsPerDWORD = 32;
			static constexpr int32 NumBitsPerDWORDLogTwo = 5;

		public:
			inline explicit FRelativeBitReference(int32 BitIndex)
				: WordIndex(BitIndex >> NumBitsPerDWORDLogTwo)
				, Mask(1 << (BitIndex & (NumBitsPerDWORD - 1)))
			{
			}

			int32  WordIndex;
			uint32 Mask;
		};

		class FSetBitIterator : public FRelativeBitReference
		{
		private:
			const ContainerImpl::FBitArray& Array;

			uint32 UnvisitedBitMask;
			int32 CurrentBitIndex;
			int32 BaseBitIndex;

		public:
			explicit FSetBitIterator(const ContainerImpl::FBitArray& InArray, int32 StartIndex = 0)
				: FRelativeBitReference(StartIndex)
				, Array(InArray)
				, UnvisitedBitMask((~0U) << (StartIndex & (NumBitsPerDWORD - 1)))
				, CurrentBitIndex(StartIndex)
				, BaseBitIndex(StartIndex & ~(NumBitsPerDWORD - 1))
			{
				if (StartIndex != Array.Num())
					FindFirstSetBit();
			}

		public:
			inline FSetBitIterator& operator++()
			{
				UnvisitedBitMask &= ~this->Mask;

				FindFirstSetBit();

				return *this;
			}

			inline explicit operator bool() const { return CurrentBitIndex < Array.Num(); }

			inline bool operator==(const FSetBitIterator& Rhs) const { return CurrentBitIndex == Rhs.CurrentBitIndex && &Array == &Rhs.Array; }
			inline bool operator!=(const FSetBitIterator& Rhs) const { return CurrentBitIndex != Rhs.CurrentBitIndex || &Array != &Rhs.Array; }

		public:
			inline int32 GetIndex() { return CurrentBitIndex; }

			void FindFirstSetBit()
			{
				const uint32* ArrayData = Array.GetData();
				const int32   ArrayNum = Array.Num();
				const int32   LastWordIndex = (ArrayNum - 1) / NumBitsPerDWORD;

				uint32 RemainingBitMask = ArrayData[this->WordIndex] & UnvisitedBitMask;
				while (!RemainingBitMask)
				{
					++this->WordIndex;
					BaseBitIndex += NumBitsPerDWORD;
					if (this->WordIndex > LastWordIndex)
					{
						CurrentBitIndex = ArrayNum;
						return;
					}

					RemainingBitMask = ArrayData[this->WordIndex];
					UnvisitedBitMask = ~0;
				}

				const uint32 NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

				this->Mask = NewRemainingBitMask ^ RemainingBitMask;

				CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - ContainerImpl::HelperFunctions::CountLeadingZeros(this->Mask);

				if (CurrentBitIndex > ArrayNum)
					CurrentBitIndex = ArrayNum;
			}
		};

		template<typename ArrayType>
		class TArrayIterator
		{
		private:
			TArray<ArrayType>& IteratedArray;
			int32 Index;

		public:
			TArrayIterator(const TArray<ArrayType>& Array, int32 StartIndex = 0x0)
				: IteratedArray(const_cast<TArray<ArrayType>&>(Array)), Index(StartIndex)
			{
			}

		public:
			inline int32 GetIndex() { return Index; }

			inline int32 IsValid() { return IteratedArray.IsValidIndex(GetIndex()); }

		public:
			inline TArrayIterator& operator++() { ++Index; return *this; }
			inline TArrayIterator& operator--() { --Index; return *this; }

			inline       ArrayType& operator*()       { return IteratedArray[GetIndex()]; }
			inline const ArrayType& operator*() const { return IteratedArray[GetIndex()]; }

			inline       ArrayType* operator->()       { return &IteratedArray[GetIndex()]; }
			inline const ArrayType* operator->() const { return &IteratedArray[GetIndex()]; }

			inline bool operator==(const TArrayIterator& Other) const { return &IteratedArray == &Other.IteratedArray && Index == Other.Index; }
			inline bool operator!=(const TArrayIterator& Other) const { return &IteratedArray != &Other.IteratedArray || Index != Other.Index; }
		};

		template<class ContainerType>
		class TContainerIterator
		{
		private:
			ContainerType& IteratedContainer;
			FSetBitIterator BitIterator;

		public:
			TContainerIterator(const ContainerType& Container, const ContainerImpl::FBitArray& BitArray, int32 StartIndex = 0x0)
				: IteratedContainer(const_cast<ContainerType&>(Container)), BitIterator(BitArray, StartIndex)
			{
			}

		public:
			inline int32 GetIndex() { return BitIterator.GetIndex(); }

			inline int32 IsValid() { return IteratedContainer.IsValidIndex(GetIndex()); }

		public:
			inline TContainerIterator& operator++() { ++BitIterator; return *this; }

			inline       auto& operator*()       { return IteratedContainer[GetIndex()]; }
			inline const auto& operator*() const { return IteratedContainer[GetIndex()]; }

			inline       auto* operator->()       { return &IteratedContainer[GetIndex()]; }
			inline const auto* operator->() const { return &IteratedContainer[GetIndex()]; }

			inline bool operator==(const TContainerIterator& Other) const { return &IteratedContainer == &Other.IteratedContainer && BitIterator == Other.BitIterator; }
			inline bool operator!=(const TContainerIterator& Other) const { return &IteratedContainer != &Other.IteratedContainer || BitIterator != Other.BitIterator; }
		};
	}

	inline Iterators::FSetBitIterator begin(const ContainerImpl::FBitArray& Array) { return Iterators::FSetBitIterator(Array, 0); }
	inline Iterators::FSetBitIterator end  (const ContainerImpl::FBitArray& Array) { return Iterators::FSetBitIterator(Array, Array.Num()); }

	template<typename T> inline Iterators::TArrayIterator<T> begin(const TArray<T>& Array) { return Iterators::TArrayIterator<T>(Array, 0); }
	template<typename T> inline Iterators::TArrayIterator<T> end  (const TArray<T>& Array) { return Iterators::TArrayIterator<T>(Array, Array.Num()); }

	template<typename T> inline Iterators::TSparseArrayIterator<T> begin(const TSparseArray<T>& Array) { return Iterators::TSparseArrayIterator<T>(Array, Array.GetAllocationFlags(), 0); }
	template<typename T> inline Iterators::TSparseArrayIterator<T> end  (const TSparseArray<T>& Array) { return Iterators::TSparseArrayIterator<T>(Array, Array.GetAllocationFlags(), Array.NumAllocated()); }

	template<typename T> inline Iterators::TSetIterator<T> begin(const TSet<T>& Set) { return Iterators::TSetIterator<T>(Set, Set.GetAllocationFlags(), 0); }
	template<typename T> inline Iterators::TSetIterator<T> end  (const TSet<T>& Set) { return Iterators::TSetIterator<T>(Set, Set.GetAllocationFlags(), Set.NumAllocated()); }

	template<typename T0, typename T1> inline Iterators::TMapIterator<T0, T1> begin(const TMap<T0, T1>& Map) { return Iterators::TMapIterator<T0, T1>(Map, Map.GetAllocationFlags(), 0); }
	template<typename T0, typename T1> inline Iterators::TMapIterator<T0, T1> end  (const TMap<T0, T1>& Map) { return Iterators::TMapIterator<T0, T1>(Map, Map.GetAllocationFlags(), Map.NumAllocated()); }

#if defined(_WIN64)
	static_assert(sizeof(TArray<int32>) == 0x10, "TArray has a wrong size!");
	static_assert(sizeof(TSet<int32>) == 0x50, "TSet has a wrong size!");
	static_assert(sizeof(TMap<int32, int32>) == 0x50, "TMap has a wrong size!");
#elif defined(_WIN32)
	static_assert(sizeof(TArray<int32>) == 0x0C, "TArray has a wrong size!");
	static_assert(sizeof(TSet<int32>) == 0x3C, "TSet has a wrong size!");
	static_assert(sizeof(TMap<int32, int32>) == 0x3C, "TMap has a wrong size!");
#endif

#ifndef IMPORT_CPP_SDK_INTO_IDA
}
#endif
)AVALONIUM_UC";
}

std::string GetPropertyFixupHeaderContent() {
    return R"(#pragma once
#include <cstdint>

class UClass;
class UObject;

template<int N>
struct FUnknownStruct
{
    uint8_t Data[(N > 0) ? N : 1];
};

template<typename T>
class TSubclassOf
{
public:
    class UClass* ClassPtr;

    inline class UClass* Get() const { return ClassPtr; }
    inline class UClass* operator->() const { return ClassPtr; }
    inline operator class UClass*() const { return ClassPtr; }
};

template<typename T>
class TWeakObjectPtr
{
public:
    int32_t ObjectIndex;
    int32_t ObjectSerialNumber;
};

template<typename T>
class TSoftObjectPtr
{
public:
    uint8_t Padding[0x28];
};

template<typename T>
class TSoftClassPtr
{
public:
    uint8_t Padding[0x28];
};

template<typename T>
class TLazyObjectPtr
{
public:
    uint8_t Padding[0x1C];
};

class FText
{
public:
    uint8_t Padding[0x18];
};

template<int N>
struct SDK_OptionalBytes
{
    uint8_t Bytes[(N > 0) ? N : 1];
};

class FScriptDelegate
{
public:
    uint8_t Padding[0x10];
};

class FMulticastScriptDelegate
{
public:
    uint8_t Padding[0x10];
};

class alignas(0x01) FMulticastSparseDelegateProperty_
{
public:
    uint8_t Padding[0x1];
};

class FScriptInterface
{
public:
    uint8_t Padding[0x10];
};

class FFieldPath
{
public:
    uint8_t Padding[0x20];
};
)" + GetFixedSizeEnumTemplateContent() + "\n";
}

}
