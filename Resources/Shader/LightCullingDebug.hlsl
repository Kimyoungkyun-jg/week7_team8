#define TILE_SIZE 32
#define INVALID_LIGHT_LIST 0xffffffffu

// C++의 FTileLightingInfo와 같은 16바이트 레이아웃이다.
cbuffer TileLightingInfo : register(b3)
{
    uint UseTiledLights;
    uint TileCountX;
    uint LightTileSize;
    uint TilePadding1;
};

// x/y는 Point 시작·개수, z/w는 Spot 시작·개수다.
StructuredBuffer<uint4> TileLightRanges : register(t9);
Texture2D<float> SceneDepth : register(t0);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    // 정점 버퍼 없이 기존 ToneMap과 같은 전체 화면 삼각형을 생성한다.
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float3 GetTileColor(uint Count)
{
    const uint ColorPeriod = 16;
    float Hue = float((Count - 1u) % ColorPeriod) / float(ColorPeriod);

    // Hue를 RGB로 변환하여 한 주기 안에서도 여러 색상을 사용한다.
    float3 RGB = saturate(
        abs(frac(Hue + float3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f) - 1.0f);
    return RGB * 0.85f + 0.10f;
}

// 0~9의 3×4 픽셀 글꼴이다. 비트 위치는 행×3+열이다.
static const uint DigitMasks[10] =
{
    3951, 3738, 3751, 3895, 2541, 3983, 3961, 1191, 3967, 2543
};

bool IsCountPixel(uint Count, uint2 InTile)
{
    // 0도 표시하며 uint의 최대 10자리까지 숫자를 그대로 출력한다.
    uint DigitCount = 1;
    for (uint Remaining = Count; Remaining >= 10; Remaining /= 10)
        ++DigitCount;

    // 네 자리마다 줄을 바꾼다. 최대 크기는 가로15·세로14픽셀이다.
    uint RowCount = (DigitCount + 3) / 4;
    uint FirstRowDigits = DigitCount - (RowCount - 1) * 4;
    uint TextHeight = RowCount * 5 - 1;
    int Y = int(InTile.y) - int((TILE_SIZE - TextHeight) / 2);
    if (Y < 0 || Y >= int(TextHeight) || uint(Y) % 5 == 4)
        return false;

    // 왼쪽 경계선 한 픽셀을 보존하면서 각 줄을 중앙에 배치한다.
    uint Row = uint(Y) / 5;
    uint RowDigits = Row == 0 ? FirstRowDigits : 4;
    uint TextWidth = RowDigits * 4 - 1;
    int X = int(InTile.x) - int((TILE_SIZE - TextWidth + 1) / 2);
    if (X < 0 || X >= int(TextWidth) || uint(X) % 4 == 3)
        return false;

    // 현재 글자의 자릿수만 추출한다.
    uint BeforeRow = Row == 0 ? 0 : FirstRowDigits + (Row - 1) * 4;
    uint DigitIndex = BeforeRow + uint(X) / 4;
    for (uint Shift = DigitCount - DigitIndex - 1; Shift > 0; --Shift)
        Count /= 10;

    uint Bit = (uint(Y) % 5) * 3 + uint(X) % 4;
    return ((DigitMasks[Count % 10] >> Bit) & 1) != 0;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    // 목록이 없거나 불투명 표면이 없는 픽셀은 기존 장면을 유지한다.
    if (UseTiledLights == 0) discard;

    uint2 Pixel = uint2(Input.Position.xy);
    if (SceneDepth.Load(int3(Pixel, 0)) >= 1.0f)
        discard;

    // 후보 광원이 있는 타일의 표면에만 표시한다.
    uint2 Tile = Pixel / TILE_SIZE;
    uint2 InTile = Pixel % TILE_SIZE;
    uint4 Range = TileLightRanges[Tile.y * TileCountX + Tile.x];
    uint Count = Range.y + Range.w;
    if (Count == 0)
        discard;

    float3 Color = GetTileColor(Count);
    float Alpha = 0.40f;

    // 경계선은 표면 위에서 더 선명하게 표시한다.
    if (InTile.x == 0 || InTile.y == 0)
    {
        Color = float3(0.08f, 0.08f, 0.08f);
        Alpha = 0.70f;
    }

    // 전체 인덱스 버퍼 부족 표식은 유지한다.
    bool PoolOverflow = Range.x == INVALID_LIGHT_LIST || Range.z == INVALID_LIGHT_LIST;
    if (PoolOverflow && InTile.x >= TILE_SIZE -3 && InTile.y >= 1 && InTile.y <= 2)
    {
        Color = float3(1.0f, 0.0f, 1.0f);
        Alpha = 1.0f;
    }

    // 실제 후보 개수를 흰색으로 표시한다.
    if (IsCountPixel(Count, InTile))
    {
        Color = float3(1.0f, 1.0f, 1.0f);
        Alpha = 1.0f;
    }

    return float4(Color, Alpha);

}
