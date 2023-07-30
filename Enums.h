#pragma once
//auto-generated

enum class SkipLinkField : unsigned __int32
{
	ArrayCount    = 1 << 3, 
	SharedDataID  = 1 << 4, 
	ObjectBlockID = 1 << 5, 
	ObjectOffset  = 1 << 6, 
	INVALID
};

enum class VertexComponentType : unsigned __int32
{
	Vertex,          
	Normal,          
	Tangent,         
	Binormal,        
	SkinnableVertex, 
	SkinnableNormal, 
	SkinnableTangent, 
	SkinnableBinormal, 
	SkinWeights,     
	SkinIndices,     
	ST,              
	Color,           
	INVALID
};

enum class PrimID : unsigned __int32
{
	Float,    
	Half,     
	UInt32,   
	UInt16,   
	UInt8,    
	UChar,    
	UNormInt16, 
	UNormInt8, 
	Int32,    
	Int16,    
	Int8,     
	Char,     
	NormInt16, 
	NormInt8, 
	Matrix3x3, 
	Matrix3x4, 
	Matrix4x4, 
	INVALID
};

enum class GUITask : unsigned __int32
{
	Main,       
	Convert,    
	Confirmation, 
	Progress,   
	INVALID
};

enum class GUIMessage : unsigned __int32
{
	Progress     = 1 << 0, 
	Confirmation = 1 << 1, 
	WindowInit   = 1 << 2, 
	Request      = 1 << 3, 
	Exit         = 1 << 4, 
	Yes          = 1 << 5, 
	No           = 1 << 6, 
	Cancel       = 1 << 7, 
	INVALID
};

inline constexpr unsigned __int32 PrimElementCount(unsigned __int32 unpacked_ID)
{
    return unpacked_ID < 48 ? unpacked_ID % 4 + 1 : 1;
}
inline constexpr PrimID PrimElementType(unsigned __int32 unpacked_ID)
{
    if (unpacked_ID > 49)
    {
        return PrimID::INVALID;
    }
    switch (unpacked_ID)
    {
        case 48: return PrimID::Matrix3x4;
        case 49: return PrimID::Matrix4x4;
        default: return PrimID(unpacked_ID >> 2);
    }
}

//EnumToString functions

inline constexpr const char * EnumToString(SkipLinkField value) noexcept
{
	switch (value)
	{
		case SkipLinkField::ArrayCount   : return "ArrayCount";
		case SkipLinkField::SharedDataID : return "SharedDataID";
		case SkipLinkField::ObjectBlockID: return "ObjectBlockID";
		case SkipLinkField::ObjectOffset : return "ObjectOffset";
	}
	return nullptr;
}

inline constexpr const char * EnumToString(VertexComponentType value) noexcept
{
	switch (value)
	{
		case VertexComponentType::Vertex           : return "Vertex";
		case VertexComponentType::Normal           : return "Normal";
		case VertexComponentType::Tangent          : return "Tangent";
		case VertexComponentType::Binormal         : return "Binormal";
		case VertexComponentType::SkinnableVertex  : return "SkinnableVertex";
		case VertexComponentType::SkinnableNormal  : return "SkinnableNormal";
		case VertexComponentType::SkinnableTangent : return "SkinnableTangent";
		case VertexComponentType::SkinnableBinormal: return "SkinnableBinormal";
		case VertexComponentType::SkinWeights      : return "SkinWeights";
		case VertexComponentType::SkinIndices      : return "SkinIndices";
		case VertexComponentType::ST               : return "ST";
		case VertexComponentType::Color            : return "Color";
	}
	return nullptr;
}

inline constexpr const char * EnumToString(PrimID value) noexcept
{
	switch (value)
	{
		case PrimID::Float     : return "Float";
		case PrimID::Half      : return "Half";
		case PrimID::UInt32    : return "UInt32";
		case PrimID::UInt16    : return "UInt16";
		case PrimID::UInt8     : return "UInt8";
		case PrimID::UChar     : return "UChar";
		case PrimID::UNormInt16: return "UNormInt16";
		case PrimID::UNormInt8 : return "UNormInt8";
		case PrimID::Int32     : return "Int32";
		case PrimID::Int16     : return "Int16";
		case PrimID::Int8      : return "Int8";
		case PrimID::Char      : return "Char";
		case PrimID::NormInt16 : return "NormInt16";
		case PrimID::NormInt8  : return "NormInt8";
		case PrimID::Matrix3x3 : return "Matrix3x3";
		case PrimID::Matrix3x4 : return "Matrix3x4";
		case PrimID::Matrix4x4 : return "Matrix4x4";
	}
	return nullptr;
}

inline constexpr const char * EnumToString(GUITask value) noexcept
{
	switch (value)
	{
		case GUITask::Main        : return "Main";
		case GUITask::Convert     : return "Convert";
		case GUITask::Confirmation: return "Confirmation";
		case GUITask::Progress    : return "Progress";
	}
	return nullptr;
}

inline constexpr const char * EnumToString(GUIMessage value) noexcept
{
	switch (value)
	{
		case GUIMessage::Progress    : return "Progress";
		case GUIMessage::Confirmation: return "Confirmation";
		case GUIMessage::WindowInit  : return "WindowInit";
		case GUIMessage::Request     : return "Request";
		case GUIMessage::Exit        : return "Exit";
		case GUIMessage::Yes         : return "Yes";
		case GUIMessage::No          : return "No";
		case GUIMessage::Cancel      : return "Cancel";
	}
	return nullptr;
}

//StringToEnum functions

template<typename EnumType>
inline constexpr EnumType StringToEnum(const char* str) noexcept {}

template<> inline constexpr SkipLinkField StringToEnum<SkipLinkField>(const char* str) noexcept
{
	if (str == nullptr)
	{
		return SkipLinkField::INVALID;
	}
	switch (str[0])
	{
	case 'O':
		if (
		    str[1] == 'b' &&
		    str[2] == 'j' &&
		    str[3] == 'e' &&
		    str[4] == 'c' &&
		    str[5] == 't'
		){
			switch (str[6])
			{
			case 'O':
				if (
				    str[7] == 'f' &&
				    str[8] == 'f' &&
				    str[9] == 's' &&
				    str[10] == 'e' &&
				    str[11] == 't' &&
				    str[12] == '\0'
				){
					return SkipLinkField::ObjectOffset;
				}
				break;
			case 'B':
				if (
				    str[7] == 'l' &&
				    str[8] == 'o' &&
				    str[9] == 'c' &&
				    str[10] == 'k' &&
				    str[11] == 'I' &&
				    str[12] == 'D' &&
				    str[13] == '\0'
				){
					return SkipLinkField::ObjectBlockID;
				}
				break;
			}
		}
		break;
	case 'S':
		if (
		    str[1] == 'h' &&
		    str[2] == 'a' &&
		    str[3] == 'r' &&
		    str[4] == 'e' &&
		    str[5] == 'd' &&
		    str[6] == 'D' &&
		    str[7] == 'a' &&
		    str[8] == 't' &&
		    str[9] == 'a' &&
		    str[10] == 'I' &&
		    str[11] == 'D' &&
		    str[12] == '\0'
		){
			return SkipLinkField::SharedDataID;
		}
		break;
	case 'A':
		if (
		    str[1] == 'r' &&
		    str[2] == 'r' &&
		    str[3] == 'a' &&
		    str[4] == 'y' &&
		    str[5] == 'C' &&
		    str[6] == 'o' &&
		    str[7] == 'u' &&
		    str[8] == 'n' &&
		    str[9] == 't' &&
		    str[10] == '\0'
		){
			return SkipLinkField::ArrayCount;
		}
		break;
	}
	return SkipLinkField::INVALID;
}

template<> inline constexpr VertexComponentType StringToEnum<VertexComponentType>(const char* str) noexcept
{
	if (str == nullptr)
	{
		return VertexComponentType::INVALID;
	}
	switch (str[0])
	{
	case 'C':
		if (
		    str[1] == 'o' &&
		    str[2] == 'l' &&
		    str[3] == 'o' &&
		    str[4] == 'r' &&
		    str[5] == '\0'
		){
			return VertexComponentType::Color;
		}
		break;
	case 'S':
		switch (str[1])
		{
		case 'T':
			if (str[2] == '\0') return VertexComponentType::ST;
			break;
		case 'k':
			if (
			    str[2] == 'i' &&
			    str[3] == 'n'
			){
				switch (str[4])
				{
				case 'I':
					if (
					    str[5] == 'n' &&
					    str[6] == 'd' &&
					    str[7] == 'i' &&
					    str[8] == 'c' &&
					    str[9] == 'e' &&
					    str[10] == 's' &&
					    str[11] == '\0'
					){
						return VertexComponentType::SkinIndices;
					}
					break;
				case 'W':
					if (
					    str[5] == 'e' &&
					    str[6] == 'i' &&
					    str[7] == 'g' &&
					    str[8] == 'h' &&
					    str[9] == 't' &&
					    str[10] == 's' &&
					    str[11] == '\0'
					){
						return VertexComponentType::SkinWeights;
					}
					break;
				case 'n':
					if (
					    str[5] == 'a' &&
					    str[6] == 'b' &&
					    str[7] == 'l' &&
					    str[8] == 'e'
					){
						switch (str[9])
						{
						case 'B':
							if (
							    str[10] == 'i' &&
							    str[11] == 'n' &&
							    str[12] == 'o' &&
							    str[13] == 'r' &&
							    str[14] == 'm' &&
							    str[15] == 'a' &&
							    str[16] == 'l' &&
							    str[17] == '\0'
							){
								return VertexComponentType::SkinnableBinormal;
							}
							break;
						case 'T':
							if (
							    str[10] == 'a' &&
							    str[11] == 'n' &&
							    str[12] == 'g' &&
							    str[13] == 'e' &&
							    str[14] == 'n' &&
							    str[15] == 't' &&
							    str[16] == '\0'
							){
								return VertexComponentType::SkinnableTangent;
							}
							break;
						case 'N':
							if (
							    str[10] == 'o' &&
							    str[11] == 'r' &&
							    str[12] == 'm' &&
							    str[13] == 'a' &&
							    str[14] == 'l' &&
							    str[15] == '\0'
							){
								return VertexComponentType::SkinnableNormal;
							}
							break;
						case 'V':
							if (
							    str[10] == 'e' &&
							    str[11] == 'r' &&
							    str[12] == 't' &&
							    str[13] == 'e' &&
							    str[14] == 'x' &&
							    str[15] == '\0'
							){
								return VertexComponentType::SkinnableVertex;
							}
							break;
						}
					}
					break;
				}
			}
			break;
		}
		break;
	case 'B':
		if (
		    str[1] == 'i' &&
		    str[2] == 'n' &&
		    str[3] == 'o' &&
		    str[4] == 'r' &&
		    str[5] == 'm' &&
		    str[6] == 'a' &&
		    str[7] == 'l' &&
		    str[8] == '\0'
		){
			return VertexComponentType::Binormal;
		}
		break;
	case 'T':
		if (
		    str[1] == 'a' &&
		    str[2] == 'n' &&
		    str[3] == 'g' &&
		    str[4] == 'e' &&
		    str[5] == 'n' &&
		    str[6] == 't' &&
		    str[7] == '\0'
		){
			return VertexComponentType::Tangent;
		}
		break;
	case 'N':
		if (
		    str[1] == 'o' &&
		    str[2] == 'r' &&
		    str[3] == 'm' &&
		    str[4] == 'a' &&
		    str[5] == 'l' &&
		    str[6] == '\0'
		){
			return VertexComponentType::Normal;
		}
		break;
	case 'V':
		if (
		    str[1] == 'e' &&
		    str[2] == 'r' &&
		    str[3] == 't' &&
		    str[4] == 'e' &&
		    str[5] == 'x' &&
		    str[6] == '\0'
		){
			return VertexComponentType::Vertex;
		}
		break;
	}
	return VertexComponentType::INVALID;
}

template<> inline constexpr PrimID StringToEnum<PrimID>(const char* str) noexcept
{
	if (str == nullptr)
	{
		return PrimID::INVALID;
	}
	switch (str[0])
	{
	case 'M':
		if (
		    str[1] == 'a' &&
		    str[2] == 't' &&
		    str[3] == 'r' &&
		    str[4] == 'i' &&
		    str[5] == 'x'
		){
			switch (str[6])
			{
			case '4':
				if (
				    str[7] == 'x' &&
				    str[8] == '4' &&
				    str[9] == '\0'
				){
					return PrimID::Matrix4x4;
				}
				break;
			case '3':
				if (str[7] == 'x')
				{
					switch (str[8])
					{
					case '4':
						if (str[9] == '\0') return PrimID::Matrix3x4;
						break;
					case '3':
						if (str[9] == '\0') return PrimID::Matrix3x3;
						break;
					}
				}
				break;
			}
		}
		break;
	case 'N':
		if (
		    str[1] == 'o' &&
		    str[2] == 'r' &&
		    str[3] == 'm' &&
		    str[4] == 'I' &&
		    str[5] == 'n' &&
		    str[6] == 't'
		){
			switch (str[7])
			{
			case '8':
				if (str[8] == '\0') return PrimID::NormInt8;
				break;
			case '1':
				if (str[8] == '6' &&
				    str[9] == '\0')
				{
					return PrimID::NormInt16;
				}
				break;
			}
		}
		break;
	case 'C':
		if (
		    str[1] == 'h' &&
		    str[2] == 'a' &&
		    str[3] == 'r' &&
		    str[4] == '\0'
		){
			return PrimID::Char;
		}
		break;
	case 'I':
		if (
		    str[1] == 'n' &&
		    str[2] == 't'
		){
			switch (str[3])
			{
			case '8':
				if (str[4] == '\0') return PrimID::Int8;
				break;
			case '1':
				if (str[4] == '6' &&
				    str[5] == '\0')
				{
					return PrimID::Int16;
				}
				break;
			case '3':
				if (str[4] == '2' &&
				    str[5] == '\0')
				{
					return PrimID::Int32;
				}
				break;
			}
		}
		break;
	case 'U':
		switch (str[1])
		{
		case 'N':
			if (
			    str[2] == 'o' &&
			    str[3] == 'r' &&
			    str[4] == 'm' &&
			    str[5] == 'I' &&
			    str[6] == 'n' &&
			    str[7] == 't'
			){
				switch (str[8])
				{
				case '8':
					if (str[9] == '\0') return PrimID::UNormInt8;
					break;
				case '1':
					if (str[9] == '6' &&
					    str[10] == '\0')
					{
						return PrimID::UNormInt16;
					}
					break;
				}
			}
			break;
		case 'C':
			if (
			    str[2] == 'h' &&
			    str[3] == 'a' &&
			    str[4] == 'r' &&
			    str[5] == '\0'
			){
				return PrimID::UChar;
			}
			break;
		case 'I':
			if (
			    str[2] == 'n' &&
			    str[3] == 't'
			){
				switch (str[4])
				{
				case '8':
					if (str[5] == '\0') return PrimID::UInt8;
					break;
				case '1':
					if (str[5] == '6' &&
					    str[6] == '\0')
					{
						return PrimID::UInt16;
					}
					break;
				case '3':
					if (str[5] == '2' &&
					    str[6] == '\0')
					{
						return PrimID::UInt32;
					}
					break;
				}
			}
			break;
		}
		break;
	case 'H':
		if (
		    str[1] == 'a' &&
		    str[2] == 'l' &&
		    str[3] == 'f' &&
		    str[4] == '\0'
		){
			return PrimID::Half;
		}
		break;
	case 'F':
		if (
		    str[1] == 'l' &&
		    str[2] == 'o' &&
		    str[3] == 'a' &&
		    str[4] == 't' &&
		    str[5] == '\0'
		){
			return PrimID::Float;
		}
		break;
	}
	return PrimID::INVALID;
}

template<> inline constexpr GUITask StringToEnum<GUITask>(const char* str) noexcept
{
	if (str == nullptr)
	{
		return GUITask::INVALID;
	}
	switch (str[0])
	{
	case 'P':
		if (
		    str[1] == 'r' &&
		    str[2] == 'o' &&
		    str[3] == 'g' &&
		    str[4] == 'r' &&
		    str[5] == 'e' &&
		    str[6] == 's' &&
		    str[7] == 's' &&
		    str[8] == '\0'
		){
			return GUITask::Progress;
		}
		break;
	case 'C':
		if (
		    str[1] == 'o' &&
		    str[2] == 'n'
		){
			switch (str[3])
			{
			case 'f':
				if (
				    str[4] == 'i' &&
				    str[5] == 'r' &&
				    str[6] == 'm' &&
				    str[7] == 'a' &&
				    str[8] == 't' &&
				    str[9] == 'i' &&
				    str[10] == 'o' &&
				    str[11] == 'n' &&
				    str[12] == '\0'
				){
					return GUITask::Confirmation;
				}
				break;
			case 'v':
				if (
				    str[4] == 'e' &&
				    str[5] == 'r' &&
				    str[6] == 't' &&
				    str[7] == '\0'
				){
					return GUITask::Convert;
				}
				break;
			}
		}
		break;
	case 'M':
		if (
		    str[1] == 'a' &&
		    str[2] == 'i' &&
		    str[3] == 'n' &&
		    str[4] == '\0'
		){
			return GUITask::Main;
		}
		break;
	}
	return GUITask::INVALID;
}

template<> inline constexpr GUIMessage StringToEnum<GUIMessage>(const char* str) noexcept
{
	if (str == nullptr)
	{
		return GUIMessage::INVALID;
	}
	switch (str[0])
	{
	case 'C':
		switch (str[1])
		{
		case 'a':
			if (
			    str[2] == 'n' &&
			    str[3] == 'c' &&
			    str[4] == 'e' &&
			    str[5] == 'l' &&
			    str[6] == '\0'
			){
				return GUIMessage::Cancel;
			}
			break;
		case 'o':
			if (
			    str[2] == 'n' &&
			    str[3] == 'f' &&
			    str[4] == 'i' &&
			    str[5] == 'r' &&
			    str[6] == 'm' &&
			    str[7] == 'a' &&
			    str[8] == 't' &&
			    str[9] == 'i' &&
			    str[10] == 'o' &&
			    str[11] == 'n' &&
			    str[12] == '\0'
			){
				return GUIMessage::Confirmation;
			}
			break;
		}
		break;
	case 'N':
		if (str[1] == 'o' &&
		    str[2] == '\0')
		{
			return GUIMessage::No;
		}
		break;
	case 'Y':
		if (
		    str[1] == 'e' &&
		    str[2] == 's' &&
		    str[3] == '\0'
		){
			return GUIMessage::Yes;
		}
		break;
	case 'E':
		if (
		    str[1] == 'x' &&
		    str[2] == 'i' &&
		    str[3] == 't' &&
		    str[4] == '\0'
		){
			return GUIMessage::Exit;
		}
		break;
	case 'R':
		if (
		    str[1] == 'e' &&
		    str[2] == 'q' &&
		    str[3] == 'u' &&
		    str[4] == 'e' &&
		    str[5] == 's' &&
		    str[6] == 't' &&
		    str[7] == '\0'
		){
			return GUIMessage::Request;
		}
		break;
	case 'W':
		if (
		    str[1] == 'i' &&
		    str[2] == 'n' &&
		    str[3] == 'd' &&
		    str[4] == 'o' &&
		    str[5] == 'w' &&
		    str[6] == 'I' &&
		    str[7] == 'n' &&
		    str[8] == 'i' &&
		    str[9] == 't' &&
		    str[10] == '\0'
		){
			return GUIMessage::WindowInit;
		}
		break;
	case 'P':
		if (
		    str[1] == 'r' &&
		    str[2] == 'o' &&
		    str[3] == 'g' &&
		    str[4] == 'r' &&
		    str[5] == 'e' &&
		    str[6] == 's' &&
		    str[7] == 's' &&
		    str[8] == '\0'
		){
			return GUIMessage::Progress;
		}
		break;
	}
	return GUIMessage::INVALID;
}