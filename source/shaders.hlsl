cbuffer Dynamic : register(b0)
{
    float4 points_num;
    float4 points_den;
    float normalization_factor;
};

cbuffer Layout : register(b1)
{
    float4 center_scale;
};

cbuffer Plot : register(b2)
{
    // NOTE: rectangle giving the visible portion of the data
    float4 plotviewport_lo_x_hi_x_lo_y_hi_y_data;
    // NOTE: rectanle giving the plot rectangle on the viewport
    float4 plotviewport_lo_x_hi_x_lo_y_hi_y_viewport;
    float4 curve_interval_x_data_margin_x_dimension_viewport_unused;
    uint num_curve_slices;
};


Texture2D ttf_font_texture : register(t1);
SamplerState ttf_font_sampler : register(s1);

static const float PI = 3.14159265f;

struct Vertex
{
    float2 position : POSITION;
};

struct WidgetVertex
{
    float4 position_screen_data : POSITION;
};

struct WidgetScreenVertex
{
    float4 position_screen : SV_POSITION;
    float2 position_data : TEXCOORD2;
};

struct ScreenVertex
{
    float4 position_screen : SV_POSITION;
    float4 color : COLOR;
};

struct FontScreenVertex
{
    float4 position_screen : SV_POSITION;
    float2 position_texture : TEXCOORD2;
};

WidgetScreenVertex circle_transform(WidgetVertex v)
{
    WidgetScreenVertex vs;

    // NOTE: data to local is identity
    float2 position_screen_local = v.position_screen_data.xy;

    float2 center = center_scale.xy;
    float scale_x = center_scale.z;
    float scale_y = center_scale.w;
    
    vs.position_screen.x = center.x + position_screen_local.x*scale_x;
    vs.position_screen.y = center.y + position_screen_local.y*scale_y;
    vs.position_screen.z = 0.0f;
    vs.position_screen.w = 1.0f;
    
    vs.position_data = v.position_screen_data.zw;

    return vs;
}

// NOTE: maps t in the interval [lo_a, hi_a] to the interval [lo_b, hi_b]
float interval_transform(float lo_a, float hi_a, float lo_b, float hi_b, float t)
{
    return
        ((lo_b - hi_b)*t + lo_a*hi_b - hi_a*lo_b) /
        (lo_a - hi_a);
}

ScreenVertex plot_transform(float curve_y_data : POSITION, uint vertex_idx: SV_VertexID)
{
    
    ScreenVertex vs;

    uint slice_idx = vertex_idx;
    uint num_segments = num_curve_slices - 1;
    
    float plotviewport_lo_x_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[0];
    float plotviewport_hi_x_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[1];
    float plotviewport_lo_y_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[2];
    float plotviewport_hi_y_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[3];

    float plotviewport_lo_x_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[0];
    float plotviewport_hi_x_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[1];
    float plotviewport_lo_y_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[2];
    float plotviewport_hi_y_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[3];

    float curve_lo_x_data = curve_interval_x_data_margin_x_dimension_viewport_unused[0];
    float curve_hi_x_data = curve_interval_x_data_margin_x_dimension_viewport_unused[1];

    float curve_lo_x_viewport =
        interval_transform(
            plotviewport_lo_x_data,
            plotviewport_hi_x_data,
            plotviewport_lo_x_viewport,
            plotviewport_hi_x_viewport,
            curve_lo_x_data
            );
                
    float curve_hi_x_viewport =
        interval_transform(
            plotviewport_lo_x_data,
            plotviewport_hi_x_data,
            plotviewport_lo_x_viewport,
            plotviewport_hi_x_viewport,
            curve_hi_x_data
            );
    
    float t = float(slice_idx) / float(num_segments);
    float curve_x_data = lerp(curve_lo_x_data, curve_hi_x_data, t);
    float curve_x_viewport =
        interval_transform(
            plotviewport_lo_x_data,
            plotviewport_hi_x_data,
            plotviewport_lo_x_viewport,
            plotviewport_hi_x_viewport,
            curve_x_data
            );

    float curve_y_viewport =
        interval_transform(
            plotviewport_lo_y_data,
            plotviewport_hi_y_data,
            plotviewport_lo_y_viewport,
            plotviewport_hi_y_viewport,
            curve_y_data
            );
    
    vs.position_screen.x = curve_x_viewport;
    vs.position_screen.y = curve_y_viewport;
    vs.position_screen.z = 0.0f;
    vs.position_screen.w = 1.0f;
    vs.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    
    return vs;    

}

float2 center_position(uint instance)
{

    int numden_idx = instance / 4;
    int conjugate_idx = instance % 2;
    int leftright_idx = (instance/2) % 2;

    float2 p[2][2] =
    {
        {
            points_num.xy,
            points_num.zw
        },
        {
            points_den.xy,
            points_den.zw
        }
    };

    float2 pos = p[numden_idx][leftright_idx];

    if(conjugate_idx == 1)
    {
        pos.y = -pos.y;
    }
    
    return pos;
    
}

float4 center_color(uint instance)
{

    float4 colors[2] =
        {
            {1, 1, 1, 1.0f},
            {0, 0, 0, 1.0f},
        };

    int numden_idx = instance/4;
    return colors[numden_idx];
    
    
}

ScreenVertex dynamic_transform(Vertex v, uint instance : SV_InstanceID)
{
    ScreenVertex vs;
    
    float s = 0.04f;
    float2 position_data = center_position(instance) + s*v.position;
    float2 position_local = position_data;

    float2 center = center_scale.xy;
    float scale_x = center_scale.z;
    float scale_y = center_scale.w;    
    
    vs.color = center_color(instance);
    vs.position_screen.xy = center + position_local*float2(scale_x, scale_y);
    vs.position_screen.z = 0.0f;
    vs.position_screen.w = 1.0f;


    return vs;
}

float2 conjugate(float2 p)
{
    p.y = -p.y;
    return p;
}

float4 density(WidgetScreenVertex sv) : SV_TARGET
{
    
    float2 p[2][2] =
        {
            {
                points_num.xy,
                points_num.zw
            },
            {
                points_den.xy,
                points_den.zw
            }
        };        
    
    float2 x = sv.position_data;

    float a00 = length(x - p[0][0]);
    float ac00 = length(x - conjugate(p[0][0]));
    float a01 = length(x - p[0][1]);
    float ac01 = length(x - conjugate(p[0][1]));
    
    float num =
        a00 * ac00 * a01 * ac01;

    float b10 = length(x - p[1][0]);
    float bc10 = length(x - conjugate(p[1][0]));
    float b11 = length(x - p[1][1]);
    float bc11 = length(x - conjugate(p[1][1]));
    
    float den =
        b10 * bc10 * b11 * bc11;
    
    float a = normalization_factor * num / den;
    
    float s = clamp(0.0f, 1.0f, a);
    float g = floor(s*10.0f)/10.0f;
    float4 color = {g*0.8f, g*0.95f, g*0.8f, 1.0f};
    if(a > 1.0f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);   
    }
    else
    {
        return color;

    }
}

float2 complex_product(float2 a, float2 b)
{
    return float2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x);
}

float complex_magnitude_squared(float2 a)
{
    return pow(a[0], 2) + pow(a[1], 2);
}

float2
complex_quotient(float2 n, float2 d)
{
    float2 q;
    float d_sq = complex_magnitude_squared(d);
    q[0] = (n[0]*d[0] + n[1]*d[1])/d_sq;
    q[1] = (n[1]*d[0] - n[0]*d[1])/d_sq;
    return q;
}

float2 complex_product_4(float2 a1, float2 a2, float2 a3, float2 a4)
{
    return complex_product(complex_product(a1, a2), complex_product(a3, a4));
}

float complex_phase(float2 a)
{
    return atan2(a.y, a.x);
}

float4 domain_coloring(WidgetScreenVertex sv) : SV_TARGET
{
    
    float2 p[2][2] =
        {
            {
                points_num.xy,
                points_num.zw
            },
            {
                points_den.xy,
                points_den.zw
            }
        };        
    
    float2 x = sv.position_data;

    float2 a00 = x - p[0][0];
    float2 ac00 = x - conjugate(p[0][0]);
    float2 a01 = x - p[0][1];
    float2 ac01 = x - conjugate(p[0][1]);
    
    float2 num =
        complex_product_4(a00, ac00, a01, ac01);

    float2 b10 = x - p[1][0];
    float2 bc10 = x - conjugate(p[1][0]);
    float2 b11 = x - p[1][1];
    float2 bc11 = x - conjugate(p[1][1]);
    
    float2 den =
        complex_product_4(b10, bc10, b11, bc11);

    float2 quotient = complex_quotient(num, den);
    float phase = complex_phase(quotient);
    float normalized_phase = frac( 0.5f*phase/PI );
    uint color_idx = floor(normalized_phase*12.0f);
    float3 colors[12] =
        {
            {  1,   0,   0}, //   0, ff 00 00 
            {  1, 0.5,   0}, //  30, ff 80 00
            {  1,   1,   0}, //  60, ff ff 00
            {0.5,   1,   0}, //  90, 80 ff 00
            {  0,   1,   0}, // 120, 00 ff 00
            {  0,   1, 0.5}, // 150, 00 ff 80
            {  0,   1,   1}, // 180, 00 ff ff
            {  0, 0.5,   1}, // 210, 00 80 ff
            {  0,   0,   1}, // 240, 00 00 ff
            {0.5,   0,   1}, // 270, 80 00 ff
            {  1,   0,   1}, // 300, ff 00 ff
            {  1,   0, 0.5}, // 330, ff 00 80
        };
    
    float4 color;    
    color.rgb = 0.5f*colors[color_idx];
    color.a = 1.0f;
    
    return color;
}

ScreenVertex colorbar_transform(uint vertex_idx: SV_VertexID)
{

    float lo_y_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[2];
    float hi_y_data = plotviewport_lo_x_hi_x_lo_y_hi_y_data[3];

    float lo_x_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[0];
    float hi_x_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[1];
    float lo_y_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[2];
    float hi_y_viewport = plotviewport_lo_x_hi_x_lo_y_hi_y_viewport[3];

    float margin_x_dimension_viewport = curve_interval_x_data_margin_x_dimension_viewport_unused[2];

    float middle_x_viewport = (lo_x_viewport + hi_x_viewport)/2.0f;
    float bar_lo_x_viewport = lo_x_viewport - 1.3f*margin_x_dimension_viewport;
    float bar_hi_x_viewport = lo_x_viewport - 1.0f*margin_x_dimension_viewport;
    
    float2 position_viewport = float2(0,0);
    if(vertex_idx==0)
    {
        position_viewport.x = bar_hi_x_viewport;
        position_viewport.y = lo_y_viewport;
    }
    else if(vertex_idx==1)
    {
        position_viewport.x = bar_hi_x_viewport;
        position_viewport.y = hi_y_viewport;
    }
    else if(vertex_idx==2)
    {
        position_viewport.x = bar_lo_x_viewport;
        position_viewport.y = lo_y_viewport;
    }
    else if(vertex_idx==3)
    {
        position_viewport.x = bar_lo_x_viewport;
        position_viewport.y = hi_y_viewport;
    }
    
    
    float y_data =
        interval_transform(lo_y_viewport, hi_y_viewport, lo_y_data, hi_y_data, position_viewport.y);
    // TODO: use y_data to compute the texture coordinates and set those
    
    ScreenVertex vertex_viewport;
    vertex_viewport.position_screen = float4(position_viewport, 0, 1);
    vertex_viewport.color = float4(y_data,0,1,0.5f);

    return vertex_viewport;
    
}

float4 solid(ScreenVertex sv) : SV_TARGET
{
    return sv.color;
}

float4 colorbar_magnitude(ScreenVertex sv) : SV_TARGET
{

    float a = sv.color.r;
    float s = clamp(0.0f, 1.0f, a);
    float g = floor(s*10.0f)/10.0f;
    float4 color = {g*0.8f, g*0.95f, g*0.8f, 1.0f};
    if(a > 1.0f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);   
    }
    else
    {
        return color;
    }
    
}

float4 colorbar_colorwheel(ScreenVertex sv) : SV_TARGET
{

    if(sv.color.r < -0.5f || sv.color.r > 0.5f)
        return float4(0,0,0,0);

    float normalized_phase = frac(sv.color.r);    
    
    uint color_idx = floor(normalized_phase*12.0f);
    float3 colors[12] =
        {
            {  1,   0,   0}, //   0, ff 00 00 
            {  1, 0.5,   0}, //  30, ff 80 00
            {  1,   1,   0}, //  60, ff ff 00
            {0.5,   1,   0}, //  90, 80 ff 00
            {  0,   1,   0}, // 120, 00 ff 00
            {  0,   1, 0.5}, // 150, 00 ff 80
            {  0,   1,   1}, // 180, 00 ff ff
            {  0, 0.5,   1}, // 210, 00 80 ff
            {  0,   0,   1}, // 240, 00 00 ff
            {0.5,   0,   1}, // 270, 80 00 ff
            {  1,   0,   1}, // 300, ff 00 ff
            {  1,   0, 0.5}, // 330, ff 00 80
        };
    
    float4 color;    
    color.rgb = 0.5f*colors[color_idx];
    color.a = 1.0f;
    
    return color;
}

FontScreenVertex
ttf_font_vertex_shader(uint vertex_idx: SV_VertexID)
{
    
    FontScreenVertex sv;

    // TODO: pass as shader constants!

    float2 pixel_dimensions_viewport = float2(2.0f/1024.0f, 2.0f/800.0f);
    
    float2 xy_pixels[4] =
        {
            float2(4.0f, 0.0f),
            float2(4.0f, 3.0f),
            float2(0.0f, 0.0f),
            float2(0.0f, 3.0f),
        };
    
    sv.position_screen =
        float4(
            xy_pixels[vertex_idx].yx*pixel_dimensions_viewport
            +
            pixel_dimensions_viewport*float2(33.0f, -80.0f),
            0.0f, 1.0f
            );

    float2 texel_dimensions_texture = float2(1.0f/8.0f, 1.0f/8.0f);
    
    float2 uv_texels[4] =
        {
            float2(5.0f, 3.0f),
            float2(5.0f, 0.0f),
            float2(1.0f, 3.0f),
            float2(1.0f, 0.0f),
        };    
    
    sv.position_texture = texel_dimensions_texture*uv_texels[vertex_idx];
    
    return sv;
    
}

float4 ttf_font_pixel_shader(FontScreenVertex sv) : SV_TARGET
{
    float4 s = ttf_font_texture.Sample(ttf_font_sampler, sv.position_texture);
    return float4(s.r, s.r, s.r, 1.0f);
}
