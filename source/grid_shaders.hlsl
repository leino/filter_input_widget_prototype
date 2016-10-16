cbuffer Grid : register(b0)
{
    float4 offset_spacing_premainder_lo;
    uint4 max_power_line_idx_offset_orientation_base;
    float hi;
};

cbuffer GridText : register(b1)
{
    uint4 number_characters[5];
    float text_end_position_viewport;
    float text_middle_transverse_position_viewport;
    float text_alpha;
    uint text_orientation;
    uint2 viewport_dimensions_pixels;
    uint end_margin_pixels;
    uint character_spacing_pixels;
};

#define FONT_NUM_CHARACTERS 14
#define FONT_CHARACTER_WIDTH_PIXELS 5
// IMPORTANT: assumed to be even
#define FONT_CHARACTER_HEIGHT_PIXELS 6

Texture2D font_texture : register(t0);
SamplerState font_sampler : register(s0);

struct GridScreenVertex
{
    float4 position_screen : SV_POSITION;
    float4 color : COLOR;
};

struct FontScreenVertex
{
    float4 position_screen : SV_POSITION;
    float2 position_texture : TEXCOORD2;
};

GridScreenVertex
grid_vertex_shader(uint instance_idx : SV_InstanceID, uint vertex_idx: SV_VertexID)
{
    GridScreenVertex vs;

    float offset = offset_spacing_premainder_lo[0];
    float spacing = offset_spacing_premainder_lo[1];
    float power_remainder = offset_spacing_premainder_lo[2];
    float lo = offset_spacing_premainder_lo[3];

    uint max_power = max_power_line_idx_offset_orientation_base[0];
    uint line_idx_offset = max_power_line_idx_offset_orientation_base[1];
    uint orientation = max_power_line_idx_offset_orientation_base[2];
    uint base = max_power_line_idx_offset_orientation_base[3];
    

    // NOTE: calculate the highest power by which this grid line is divisible
    uint power = 0;
    {
        uint n = instance_idx + line_idx_offset;
        while( (n % base) == 0)
        {
            n /= base;
            power++;
            if(power == max_power)
                break;
        }
    }

    if(orientation == GRID_ORIENTATION_HORIZONTAL)
    {

        vs.position_screen.xy =         
            float2(
                lerp(lo, hi, float(vertex_idx)),
                offset + float(instance_idx)*spacing
                );

    }
    else
    {

        vs.position_screen.xy = 
            float2(
                offset + float(instance_idx)*spacing,
                lerp(lo, hi, float(vertex_idx))
                );

    }
    vs.position_screen.z = 0.0f;
    vs.position_screen.w = 1.0f;

    float alpha = (float(power) + power_remainder)/float(max_power+1.0f);
    vs.color = float4(1.0f, 1.0f, 1.0f, alpha);
    return vs;
}

FontScreenVertex
font_vertex_shader(uint instance_idx : SV_InstanceID, uint vertex_idx: SV_VertexID)
{
    
    FontScreenVertex sv;

    float character_idx = float(number_characters[instance_idx/4][instance_idx%4]);
    
    float character_width_texture = 1.0f/float(FONT_NUM_CHARACTERS);
    float character_height_texture = 1.0f;

    // TODO: pass as shader constants!
    float pixel_width_viewport = 2.0f/viewport_dimensions_pixels.x;
    float pixel_height_viewport = 2.0f/viewport_dimensions_pixels.y;

    float character_width_pixels = float(FONT_CHARACTER_WIDTH_PIXELS);
    float character_height_pixels = float(FONT_CHARACTER_HEIGHT_PIXELS); 
    
    if(text_orientation == GRID_ORIENTATION_VERTICAL)
    {

        float2 offset_pixels =
            float2(
                0.0f,
                -(character_width_pixels*float(instance_idx+1) + float(character_spacing_pixels)*float(instance_idx))
                )
            +
            float2(0.0f, -float(end_margin_pixels));
        

        float2 start_position_viewport =
            float2(text_middle_transverse_position_viewport, text_end_position_viewport);
    
        float2 character_center_viewport =
            start_position_viewport +
            offset_pixels*float2(pixel_width_viewport, pixel_height_viewport);
    
        float2 xy[4] =
            {
                float2(+1.0f, 1.0f),
                float2(-1.0f, 1.0f),
                float2(+1.0f, 0.0f),
                float2(-1.0f, 0.0f),
            };
            
        float2 position_screen =
            character_center_viewport +
            float2(
                character_width_pixels*pixel_height_viewport/2.0f,
                character_height_pixels * pixel_width_viewport
                )*xy[vertex_idx]
            ;
        sv.position_screen = float4(position_screen, 0.0f, 1.0f);

        float2 uv[4] =
            {
                float2(
                    (character_idx + 1.0f)*character_width_texture,
                    1.0f
                    ),
                float2(
                    (character_idx + 1.0f)*character_width_texture,
                    0.0f
                    ),
                float2(
                    (character_idx + 0.0f)*character_width_texture,
                    1.0f
                    ),
                float2(
                    (character_idx + 0.0f)*character_width_texture,
                    0.0f
                    ),
            };
        sv.position_texture = uv[vertex_idx];
        return sv;
    }
    else
    {
        
        float2 offset_pixels =
            float2(
                -(character_width_pixels*float(instance_idx+1) + float(character_spacing_pixels)*float(instance_idx)),
                0.0f
                )
            +
            float2(-float(end_margin_pixels), 0.0f);

        float2 start_position_viewport =
            float2(text_end_position_viewport, text_middle_transverse_position_viewport);
    
        float2 character_center_viewport =
            start_position_viewport +
            offset_pixels*float2(pixel_width_viewport, pixel_height_viewport);
    
        float2 xy[4] =
        {
            float2(1.0f, -1.0f),
            float2(1.0f, +1.0f),
            float2(0.0f, -1.0f),
            float2(0.0f, +1.0f),
        };
        
        float2 position_screen =
            character_center_viewport +
            float2(
                character_width_pixels * pixel_width_viewport,
                character_height_pixels*pixel_height_viewport/2.0f
                ) * xy[vertex_idx];
        sv.position_screen = float4(position_screen, 0.0f, 1.0f);

        float2 uv[4] =
            {
                float2(
                    (character_idx + 1.0f)*character_width_texture,
                    1.0f
                    ),
                float2(
                    (character_idx + 1.0f)*character_width_texture,
                    0.0f
                    ),
                float2(
                    (character_idx + 0.0f)*character_width_texture,
                    1.0f
                    ),
                float2(
                    (character_idx + 0.0f)*character_width_texture,
                    0.0f
                    ),
            };
        sv.position_texture = uv[vertex_idx];
        return sv;        
        
    }
    
}

float4
font_pixel_shader(FontScreenVertex sv) : SV_TARGET
{
    float4 s = font_texture.Sample(font_sampler, sv.position_texture);
    return float4(s.r, s.r, s.r, text_alpha*s.r);
}
