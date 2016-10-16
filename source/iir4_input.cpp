#include <windows.h>
#include <d3d11.h>

#define _USE_MATH_DEFINES
#include <math.h>
#undef _USE_MATH_DEFINES
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include "ifdef_sanity_checks.h"
#include "integer.h"
#include "numbers.cpp"
#include "numerics.cpp"
#include "linalg.cpp"
#include "platform.hpp"
#include "array.h"
#include "complex.cpp"
#include "log.h"
#include "geometry_2.cpp"

LRESULT CALLBACK
window_callback(
    HWND window_handle,
    UINT message,
    WPARAM wparam,
    LPARAM lparam);

static HWND g_window_handle;
#include "win32_platform.cpp"

#define GRID_LOG_ERROR(msg) Platform::log_line_string(msg)
#include "grid.cpp"
#include "grid_render_d3d11.cpp"

int const NUM_RADIAL_SEGMENTS = 100;

// NOTE: This is must match vertex input layout
struct ShapeVertex
{
    Vec2::Vec2 position;
};

// NOTE: This is must match vertex input layout
// NOTE: This is the type of vertex used for triangles displaying the widget.
struct WidgetVertex
{
    Vec2::Vec2 position_screen;
    Vec2::Vec2 position_data;
};

struct WidgetLayoutConstants
{
    float center_x_position_viewport;
    float center_y_position_viewport;
    float data_x_unit_viewport; 
    float data_y_unit_viewport;
};

// NOTE: This must match the constant buffer in the shader, be careful about padding!
struct DynamicConstants
{
    Vec2::Vec2 points_num[2];
    Vec2::Vec2 points_den[2];
    float normalization_factor;
    float __padding[3];
};

// NOTE: This must match the constant buffer in the shader, be careful about padding!
struct PlotConstants
{
    float plotviewport_data[4]; // x lo, x hi, y lo, y hi
    float plotviewport_viewport[4]; // x lo, x hi, y lo, y hi
    float curve_interval_x_data[2]; float margin_x_dimension_viewport; float __padding_1[1];
    uint num_curve_slices; float __padding_2[3];
};
static_assert(sizeof(PlotConstants) == sizeof(float[4])*4, "stuff");

union Parameters
{
    struct
    {
        Complex::C zero[2];
        Complex::C pole[2];
    } parameter;

    Complex::C parameters[4];

    // Cheesy name: numerATOR, denumiarATOR -- these are the 'factors' of the 'ators' of the transfer function!
    Complex::C ator_factors[2][2];
    
};

// NOTE: normalization constant suitable for lowpass filters
inline float
normalization_constant_lowpass(Parameters const*const parameters)
{
    using namespace Complex;

    C unit;
    unit.component.real = 1.0f;
    unit.component.imaginary = 0.0f;

    C d[2][2];
    for(int i=0; i<2; i++)
    {
        for(int j=0; j<2; j++)
        {
            difference(&unit, &parameters->ator_factors[i][j], &d[i][j]);
        }
    }
    
    return
        (magnitude_squared(&d[1][0])*magnitude_squared(&d[1][1]))/
        (magnitude_squared(&d[0][0])*magnitude_squared(&d[0][1]));
}

// NOTE: normalization constant suitable for highpass
inline float
normalization_constant_highpass(Parameters const*const parameters)
{
    using namespace Complex;

    C negative_unit;
    negative_unit.component.real = -1.0f;
    negative_unit.component.imaginary = 0.0f;

    C d[2][2];
    for(int i=0; i<2; i++)
    {
        for(int j=0; j<2; j++)
        {
            difference(&negative_unit, &parameters->ator_factors[i][j], &d[i][j]);
        }
    }
    
    return
        (magnitude_squared(&d[1][0])*magnitude_squared(&d[1][1]))/
        (magnitude_squared(&d[0][0])*magnitude_squared(&d[0][1]));
}

LRESULT CALLBACK
window_callback(
    HWND window_handle,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{

    switch(message)
    {
    case WM_SIZE:
    {
        return 0;
    } break;
        
    case WM_CLOSE:
    {
        DestroyWindow(window_handle);
        return 0;    
    } break;
        
    case WM_ACTIVATEAPP:
    {
        return 0;
    } break;
        
    case WM_DESTROY:
    {
        int exit_code = 0;
        PostQuitMessage(exit_code);
        return 0;
    } break;


    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        Platform::log_line_string("keyboard input came in through a non-dispatch message");
        assert(false);
        return 0;
    } break;    
    
    default:
    {
        LRESULT result = DefWindowProc(window_handle, message, wparam, lparam);
        return result;
    } break;
    }

}

bool create_circle_vertex_buffer(
    ID3D11Device* d3d_device,
    ID3D11Buffer**const vertex_buffer
    )
{

    assert( vertex_buffer != 0 );

    int const num_radial_slices = NUM_RADIAL_SEGMENTS;
    int const num_vertices = num_radial_slices + 1;
    
    WidgetVertex vertices[num_vertices] = {};

    {
        int const idx = 0;
        WidgetVertex *const v = &vertices[idx];
        v->position_screen = {0.0f, 0.0f};
        v->position_data = {0.0f, 0.0f};
    }
    
    for(int slice_idx = 0; slice_idx < num_radial_slices; slice_idx++)
    {
        
        using namespace Numerics;
        WidgetVertex *const v = &vertices[slice_idx];
        float const angle = float(slice_idx) * 2.0f*PI_FLOAT/float(NUM_RADIAL_SEGMENTS);
        v->position_screen = {cos(angle), sin(angle)};
        v->position_data = v->position_screen;

    }    
    
    {            
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(WidgetVertex)*num_vertices;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(WidgetVertex)
        description.StructureByteStride = 0;
            
        D3D11_SUBRESOURCE_DATA initial_data = {};
        initial_data.pSysMem = vertices;
            
        HRESULT result = d3d_device->CreateBuffer(&description, &initial_data, vertex_buffer);

        if( FAILED(result) )
        {
            return false;
        }
        
    }
    assert( *vertex_buffer != 0 );
    
    return true;
}

bool
create_curve_vertex_buffer(
    uint const num_vertices,
    ID3D11Device* d3d_device,
    ID3D11Buffer**const vertex_buffer
    )
{

    assert( vertex_buffer != 0 );

    {            
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(float)*num_vertices;
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(WidgetVertex)
        description.StructureByteStride = 0;
            
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
            
        HRESULT result = d3d_device->CreateBuffer(&description, initial_data, vertex_buffer);

        if( FAILED(result) )
        {
            return false;
        }
        
    }
    assert( *vertex_buffer != 0 );
    
    return true;
}

bool create_marker_vertex_buffer(
    ID3D11Device* d3d_device,
    ID3D11Buffer**const vertex_buffer
    )
{

    using namespace Numerics;
    
    assert( vertex_buffer != 0 );

    ShapeVertex vertices[6] = {};
    {
        int const idx = 0;
        ShapeVertex *const v = &vertices[idx];
        v->position.coordinates.x = +0.0f;
        v->position.coordinates.y = +0.0f;
    }

    for(int corner_idx=0; corner_idx < 5; corner_idx++)
    {
        int const vertex_idx = 1 + corner_idx;
        ShapeVertex *const v = &vertices[vertex_idx];
        float const angle = 2.0f*PI_FLOAT * float(corner_idx) / 5.0f;
        v->position.coordinates.x = 0.5f*cos(angle);
        v->position.coordinates.y = 0.5f*sin(angle);
    }
    
    {            
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(ShapeVertex)*6;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(ShapeVertex)
        description.StructureByteStride = 0;
            
        D3D11_SUBRESOURCE_DATA initial_data = {};
        initial_data.pSysMem = vertices;
            
        HRESULT result = d3d_device->CreateBuffer(&description, &initial_data, vertex_buffer);

        if( FAILED(result) )
        {
            return false;
        }
        
    }
    assert( *vertex_buffer != 0 );
    
    return true;
}


bool create_circle_index_buffer(
    ID3D11Device *const d3d_device,
    ID3D11Buffer**const buffer
    )
{

    assert( buffer != 0 );

    int const num_indices = NUM_RADIAL_SEGMENTS*3;
    int const num_slices = NUM_RADIAL_SEGMENTS;
    
    int indices[num_indices] = {};

    for(int segment_idx=0; segment_idx < NUM_RADIAL_SEGMENTS; segment_idx++)
    {
        using namespace Numerics;
        int const slice_idx_lo = remainder(num_slices, segment_idx + 0);
        int const slice_idx_hi = remainder(num_slices, segment_idx + 1);
        indices[segment_idx*3 + 0] = 0;
        indices[segment_idx*3 + 1] = 1 + slice_idx_lo;
        indices[segment_idx*3 + 2] = 1 + slice_idx_hi;
    }
    
    {            
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(int)*num_indices;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_INDEX_BUFFER;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.StructureByteStride = 0;
            
        D3D11_SUBRESOURCE_DATA initial_data = {};
        initial_data.pSysMem = indices;
            
        HRESULT result = d3d_device->CreateBuffer(&description, &initial_data, buffer);

        if( FAILED(result) )
        {
            return false;
        }
        
    }
    assert( *buffer != 0 );
    
    return true;
}

bool create_marker_index_buffer(
    ID3D11Device *const d3d_device,
    ID3D11Buffer**const buffer
    )
{

    // TODO: would be neater with a triangle strip?
    
    assert( buffer != 0 );

    int const indices[] =
        {
            0,1,2,
            0,2,3,
            0,3,4,
            0,4,5,
            0,5,1
        };

    assert(ARRAY_LENGTH(indices) == 15);
    
    {            
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(int)*ARRAY_LENGTH(indices);
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_INDEX_BUFFER;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.StructureByteStride = 0;
            
        D3D11_SUBRESOURCE_DATA initial_data = {};
        initial_data.pSysMem = indices;
            
        HRESULT result = d3d_device->CreateBuffer(&description, &initial_data, buffer);

        if( FAILED(result) )
        {
            return false;
        }
        
    }
    assert( *buffer != 0 );
    
    return true;
}

bool update_parameters(
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11Buffer *const dynamic_constant_buffer,
    Parameters const*const parameters,
    float const normalization_factor
    )
{
    ID3D11Resource* resource = dynamic_constant_buffer;
    uint subresource = 0;
    D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
    // OPTIMIZE: Could use D3D11_MAP_FLAG_DO_NOT_WAIT here, in order to not wait on GPU here.
    // Could also "doubble-buffer" the matrix constant buffer in
    // order to be sure that we don't have to wait on GPU here.
    // (At this point, GPU may not have finished executing the draw commands from previous frame yet)
    // TODO: sprinkle time_get_count() perf measurments troughtout rendering code to see where
    // we're waiting on GPU
    uint map_flags = 0;
    D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            
    HRESULT result = d3d_device_context->Map(
        resource,
        subresource,
        map_type,
        map_flags,
        &mapped_subresource
        );

    if( FAILED(result) )
    {
        // TODO: what's appropriate release mode behaviour here? check docs for return value possibilities
        Platform::log_line_string("failed to update the dynamic constant buffer");
        assert(false);
        return false;
    }
            
    DynamicConstants *const constants = (DynamicConstants*)mapped_subresource.pData;
    constants->points_num[0] = *(Vec2::Vec2*)(&parameters->parameter.zero[0].components);
    constants->points_num[1] = *(Vec2::Vec2*)(&parameters->parameter.zero[1].components);
    constants->points_den[0] = *(Vec2::Vec2*)(&parameters->parameter.pole[0].components);
    constants->points_den[1] = *(Vec2::Vec2*)(&parameters->parameter.pole[1].components);
    constants->normalization_factor = normalization_factor;

    d3d_device_context->Unmap(resource, subresource);

    return true;
}

bool update_plot_constants(
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11Buffer *const constant_buffer,
    PlotConstants const*const new_constants
    )
{
    ID3D11Resource* resource = constant_buffer;
    uint subresource = 0;
    D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
    // OPTIMIZE: Could use D3D11_MAP_FLAG_DO_NOT_WAIT here, in order to not wait on GPU here.
    // Could also "doubble-buffer" the matrix constant buffer in
    // order to be sure that we don't have to wait on GPU here.
    // (At this point, GPU may not have finished executing the draw commands from previous frame yet)
    // TODO: sprinkle time_get_count() perf measurments troughtout rendering code to see where
    // we're waiting on GPU
    uint map_flags = 0;
    D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            
    HRESULT result = d3d_device_context->Map(
        resource,
        subresource,
        map_type,
        map_flags,
        &mapped_subresource
        );

    if( FAILED(result) )
    {
        // TODO: what's appropriate release mode behaviour here? check docs for return value possibilities
        Platform::log_line_string("failed to update the plot constant buffer");
        assert(false);
        return false;
    }
            
    PlotConstants *const constants = (PlotConstants*)mapped_subresource.pData;
    *constants = *new_constants;

    d3d_device_context->Unmap(resource, subresource);

    return true;
}

bool update_layout_constants(
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11Buffer *const constant_buffer,
    WidgetLayoutConstants const*const new_constants
    )
{
    ID3D11Resource* resource = constant_buffer;
    uint subresource = 0;
    D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
    // OPTIMIZE: Could use D3D11_MAP_FLAG_DO_NOT_WAIT here, in order to not wait on GPU here.
    // Could also "doubble-buffer" the matrix constant buffer in
    // order to be sure that we don't have to wait on GPU here.
    // (At this point, GPU may not have finished executing the draw commands from previous frame yet)
    // TODO: sprinkle time_get_count() perf measurments troughtout rendering code to see where
    // we're waiting on GPU
    uint map_flags = 0;
    D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            
    HRESULT result = d3d_device_context->Map(
        resource,
        subresource,
        map_type,
        map_flags,
        &mapped_subresource
        );

    if( FAILED(result) )
    {
        // TODO: what's appropriate release mode behaviour here? check docs for return value possibilities
        Platform::log_line_string("failed to update the layout constant buffer");
        assert(false);
        return false;
    }
            
    WidgetLayoutConstants *const constants = (WidgetLayoutConstants*)mapped_subresource.pData;
    *constants = *new_constants;
    d3d_device_context->Unmap(resource, subresource);

    return true;
}

void draw_circle(
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11Buffer *const vertex_buffer,
    ID3D11Buffer* index_buffer,
    ID3D11InputLayout *const vertex_input_layout,
    ID3D11VertexShader *const vertex_shader
    )
{

    d3d_device_context->IASetInputLayout(vertex_input_layout);
                
    {
        uint num_class_instances = 0;
        ID3D11ClassInstance** class_instances = 0;
        d3d_device_context->VSSetShader(
            vertex_shader,
            class_instances,
            num_class_instances
            );
    }
    
    
    {
        uint input_slot = 0;
        uint const num_buffers = 1;
        ID3D11Buffer* buffers[num_buffers] = {vertex_buffer};
        uint strides[num_buffers] = {sizeof(WidgetVertex)};
        uint offsets[num_buffers] = {0};
        d3d_device_context->IASetVertexBuffers(
            input_slot,
            num_buffers,
            buffers,
            strides,
            offsets
            );
    }

    {

        DXGI_FORMAT const format = DXGI_FORMAT_R32_UINT;
        uint const offset = 0;
                
        d3d_device_context->IASetIndexBuffer(
            index_buffer,
            format,
            offset
            );
                
    }
            
            
    {

        uint index_count = 3*NUM_RADIAL_SEGMENTS;
        uint start_index_location = 0;
        int base_vertex_location = 0;
                
        d3d_device_context->DrawIndexed(
            index_count,
            start_index_location,
            base_vertex_location
            );
                
    }
    
    
}


void draw_markers(
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11InputLayout *const dynamic_vertex_input_layout,
    ID3D11VertexShader *const dynamic_vertex_shader,
    ID3D11Buffer* marker_vertex_buffer,
    ID3D11Buffer* marker_index_buffer,
    ID3D11PixelShader* solid_pixel_shader
    
    )
{

    { // markers

        // NOTE: set the density pixel shader
        {
            uint num_class_instances = 0;
            ID3D11ClassInstance** class_instances = 0;
            d3d_device_context->PSSetShader(
                solid_pixel_shader,
                class_instances,
                num_class_instances
                );            
        }
            
            
        d3d_device_context->IASetInputLayout(dynamic_vertex_input_layout);
                
        {
            uint num_class_instances = 0;
            ID3D11ClassInstance** class_instances = 0;
            d3d_device_context->VSSetShader(
                dynamic_vertex_shader,
                class_instances,
                num_class_instances
                );
        }            
            
        {
            uint input_slot = 0;
            uint const num_buffers = 1;
            ID3D11Buffer* buffers[num_buffers] = {marker_vertex_buffer};
            uint strides[num_buffers] = {sizeof(ShapeVertex)};
            uint offsets[num_buffers] = {0};
            d3d_device_context->IASetVertexBuffers(
                input_slot,
                num_buffers,
                buffers,
                strides,
                offsets
                );
        }

        {

            DXGI_FORMAT const format = DXGI_FORMAT_R32_UINT;
            uint const offset = 0;
                
            d3d_device_context->IASetIndexBuffer(
                marker_index_buffer,
                format,
                offset
                );
                
        }
            
            

        {

            uint const instance_index_count = 15;
            uint const instance_count = 8;
            uint const start_index_location = 0;
            int const base_vertex_location = 0;
            uint const start_instance_location = 0;
                
            d3d_device_context->DrawIndexedInstanced(
                instance_index_count,
                instance_count,
                start_index_location,
                base_vertex_location,
                start_instance_location
                );
                
                
        }
            
    }        
    
    
}

struct Interval
{
    float lo;
    float hi;
};

struct Transform
{
    Interval from;
    Interval to;
};

// NOTE: maps t in the interval [lo_a, hi_a] to the interval [lo_b, hi_b]
float
interval_transform(
    float lo_a,
    float hi_a,
    float lo_b,
    float hi_b,
    float t
    )
{
    return
        ((lo_b - hi_b)*t + lo_a*hi_b - hi_a*lo_b) /
        (lo_a - hi_a);
}

bool
try_upload_curve_vertices(
    uint const num_vertices,
    float const*const vertices,
    ID3D11DeviceContext *const d3d_device_context,
    ID3D11Buffer *const vertex_buffer
    )
{

    ID3D11Resource *const resource = vertex_buffer;
    UINT const subresource = 0;
    D3D11_MAP const type = D3D11_MAP_WRITE_DISCARD;
    UINT const flags = 0;
    D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
    
    HRESULT const result = d3d_device_context->Map(
        resource,
        subresource,
        type,
        flags,
        &mapped_subresource
        );

    if(FAILED(result))
    {
        return false;
    }

    float *const mapped_vertices = (float*)mapped_subresource.pData;
    memcpy(mapped_vertices, vertices, sizeof(float)*num_vertices);

    d3d_device_context->Unmap(resource, subresource);
    
    return true;
    
}

int CALLBACK
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int num_cmd_show)
{
    num_cmd_show; cmd_line; prev_instance;
    g_instance = instance;

    float const zoom_step_size = 0.06f;
    uint const widgetviewport_x_dimension_screen = 400;
    uint const widgetviewport_y_dimension_screen = 400;    
    uint const viewport_x_dimension_screen = 1024;
    uint const viewport_y_dimension_screen = widgetviewport_y_dimension_screen*2;
    uint const plotviewportmargin_x_dimension_characters = 8;
    uint const plotviewportmargin_y_dimension_characters = 8;
    uint const character_spacing_screen = 1;
    uint const plotviewportmargin_x_dimension_screen =
        Grid::message_width_screen(character_spacing_screen, plotviewportmargin_x_dimension_characters);
    uint const plotviewportmargin_y_dimension_screen =
        Grid::message_width_screen(character_spacing_screen, plotviewportmargin_y_dimension_characters);
    float const plotviewport_x_dimension_screen =
        0.7f*(viewport_x_dimension_screen - widgetviewport_x_dimension_screen);
    float const plotviewport_y_dimension_screen =
        float(widgetviewport_y_dimension_screen) - float(plotviewportmargin_y_dimension_screen);
    float const viewport_x_dimension_viewport = 2.0f;
    float const viewport_y_dimension_viewport = 2.0f;
    float const plotviewport_y_dimension_viewport =
        viewport_x_dimension_viewport*float(plotviewport_y_dimension_screen)/float(viewport_y_dimension_screen);
    float const plotviewport_x_dimension_viewport =
        viewport_y_dimension_viewport*float(plotviewport_x_dimension_screen)/float(viewport_x_dimension_screen);
    uint const grid_base = 10;
    float const widgetviewport_x_dimension_viewport =
        viewport_x_dimension_viewport*float(widgetviewport_x_dimension_screen)/float(viewport_x_dimension_screen);
    float const widgetviewport_y_dimension_viewport =
        viewport_y_dimension_viewport*float(widgetviewport_y_dimension_screen)/float(viewport_y_dimension_screen);

    float const viewport_x_min_viewport = -1.0f;
    float const viewport_x_max_viewport = +1.0f;
    float const viewport_y_min_viewport = -1.0f;
    float const viewport_y_max_viewport = +1.0f;
    
    float const widgetviewport_center_x_viewport = viewport_x_min_viewport + widgetviewport_x_dimension_viewport*0.5f;
    float const top_widgetviewport_center_y_viewport = +0.5f;
    float const bottom_widgetviewport_center_y_viewport = -0.5f;
    float const viewport_x_dimension_pixels = float(viewport_x_dimension_screen);
    float const viewport_y_dimension_pixels = float(viewport_y_dimension_screen);
    float const screen_x_unit_viewport = 2.0f/float(viewport_x_dimension_screen);
    float const screen_y_unit_viewport = 2.0f/float(viewport_y_dimension_screen);
    float const viewport_x_unit_screen = 1.0f/screen_x_unit_viewport;
    float const viewport_y_unit_screen = 1.0f/screen_y_unit_viewport;
    float const plotviewportmargin_x_dimension_viewport =
        screen_x_unit_viewport * float(plotviewportmargin_x_dimension_screen);
    float const plotviewportmargin_y_dimension_viewport =
        plotviewportmargin_y_dimension_screen * screen_y_unit_viewport;
    
    
    uint const num_curve_slices = 400; //uint(plotviewport_x_dimension_screen); // NOTE: one sample per pixel
    uint const num_curve_segments = num_curve_slices - 1;
    
    bool const windowed = true;
    uint const desired_refresh_rate_hz = 60;
    float const target_frame_duration = 1.0f/(float)desired_refresh_rate_hz;    

    bool mouse_input_initially_enabled = false;

    float const plotviewport_center_x_viewport =
        -1.0f + widgetviewport_x_dimension_viewport + (2.0f - widgetviewport_x_dimension_viewport)/2.0f;
    float const plotviewport_max_x_viewport = plotviewport_center_x_viewport + plotviewport_x_dimension_viewport/2.0f;
    float const plotviewport_min_x_viewport = plotviewport_center_x_viewport - plotviewport_x_dimension_viewport/2.0f;

    float const plotviewport_max_y_viewport[2] =
        {
            +1.0f,
            +0.0,
        };
    float const plotviewport_min_y_viewport[2] =
        {
            +1.0f - plotviewport_y_dimension_viewport,
            +0.0f - plotviewport_y_dimension_viewport,
        };

    float const plotviewport_min_x_screen = (plotviewport_min_x_viewport + 1.0f) * viewport_x_unit_screen;
    float const plotviewport_max_x_screen = (plotviewport_max_x_viewport + 1.0f) * viewport_x_unit_screen;
    
    {
        bool success =
            Platform::init(viewport_x_dimension_screen, viewport_y_dimension_screen, mouse_input_initially_enabled);
        if( !success )
        {
            Platform::log_line_string("platform initialization failed");
            return 0 ;
        }
    }
    
    IDXGISwapChain* swap_chain = 0;
    ID3D11Device* d3d_device = 0;
    ID3D11DeviceContext* d3d_device_context = 0;
    {
        IDXGIAdapter* adapter = 0;
        D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;
        HMODULE software_rasterization_module = 0;
        
#if IIR4_WIDGET_BUILDTYPE == IIR4_WIDGET_BUILDTYPE_RELEASE
        UINT flags = D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#elif IIR4_WIDGET_BUILDTYPE == IIR4_WIDGET_BUILDTYPE_INTERNAL
        UINT flags = 0;
        flags |= D3D11_CREATE_DEVICE_DEBUG;
        // TODO: try to turn this on
        // NOTE: needed for shader debugging
        // NOTE: if device does not support shader debugging, this will cause device creation to fail!
        // TODO: need a way of querying if driver supports shader debugging
        // flags |= D3D11_CREATE_DEVICE_DEBUGGABLE
        // NOTE: this may simplify debugging
        // flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
#else
#error Unrecognized build type.
#endif
        
        UINT sdk_version = D3D11_SDK_VERSION;
        
        DXGI_SWAP_CHAIN_DESC swap_chain_description = {};
        swap_chain_description.BufferDesc.Width = viewport_x_dimension_screen;
        swap_chain_description.BufferDesc.Height = viewport_y_dimension_screen;
        // NOTE: refresh rate has no effect in windowed mode
        swap_chain_description.BufferDesc.RefreshRate.Numerator = desired_refresh_rate_hz;
        swap_chain_description.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_description.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
        // TODO: play around with these, they control anti-aliasing, etc...
        swap_chain_description.SampleDesc.Count = 1;
        swap_chain_description.SampleDesc.Quality = 0;
        swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        // NOTE: if in fullscreen mode, the front buffer is counted, otherwise not
        swap_chain_description.BufferCount = windowed ? 1 : 2;
        swap_chain_description.OutputWindow = g_window_handle;
        swap_chain_description.Windowed = windowed ? TRUE : FALSE;
        swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        // STUDY: DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        swap_chain_description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0};
        UINT num_feature_levels = ARRAY_LENGTH(feature_levels);
        assert(num_feature_levels == 1);

        D3D_FEATURE_LEVEL first_supported_feature_level;
        
        HRESULT result =
            D3D11CreateDeviceAndSwapChain(
                adapter,
                driver_type,
                software_rasterization_module,
                flags,
                feature_levels,
                num_feature_levels,
                sdk_version,
                &swap_chain_description,
                &swap_chain,
                &d3d_device,
                &first_supported_feature_level,
                &d3d_device_context
                );
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to create d3d device and swap chain: ");
            return 0;
        }        

        // TODO: figure out how to use the d3d debug layer
    }
    assert( swap_chain != 0 );
    assert( d3d_device != 0 );
    assert( d3d_device_context != 0 );

    // This blend state is for rendering the main scene geometry
    ID3D11BlendState* blend_state = 0;
    {
        D3D11_BLEND_DESC description = {};
        description.AlphaToCoverageEnable = FALSE;
        description.IndependentBlendEnable = FALSE;
        for(int i=0; i<8; ++i)
        {
            D3D11_RENDER_TARGET_BLEND_DESC* d = &description.RenderTarget[i];
            d->BlendEnable = TRUE;
            d->SrcBlend = D3D11_BLEND_SRC_ALPHA;
            d->DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            d->BlendOp = D3D11_BLEND_OP_ADD;
            d->SrcBlendAlpha = D3D11_BLEND_ONE;
            d->DestBlendAlpha = D3D11_BLEND_ZERO;
            d->BlendOpAlpha = D3D11_BLEND_OP_ADD;
            d->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        HRESULT result = d3d_device->CreateBlendState(&description, &blend_state);
        if( FAILED(result) )
        {
            Platform::log_line_string("failed to create the color blend state");
            return 0;
        }
    }
    assert(blend_state != 0);
    
    {
        ID3D11RasterizerState* rasterizer_state = 0;
        
        {
            D3D11_RASTERIZER_DESC description = {};

            description.FillMode = D3D11_FILL_SOLID;
            description.CullMode = D3D11_CULL_NONE;
            description.FrontCounterClockwise = TRUE;
            description.DepthBias = 0;
            description.DepthBiasClamp = 0.0f;
            description.SlopeScaledDepthBias = 0.0f;
            description.DepthClipEnable = FALSE;
            description.ScissorEnable = TRUE;
            description.MultisampleEnable = FALSE;
            description.AntialiasedLineEnable = TRUE;
            
            HRESULT result =
                d3d_device->CreateRasterizerState(
                    &description,
                    &rasterizer_state
                    );
            
            if( FAILED(result) )
            {
                Platform::log_line_string("failed to create rasterizer state");
                return 0 ;
            }
        }
        assert(rasterizer_state != 0);
        
        d3d_device_context->RSSetState(rasterizer_state);
        rasterizer_state->Release();
    }

    // Get a render target view on the back buffer
    ID3D11RenderTargetView* render_target_view = 0;
    {
        
        ID3D11Texture2D* back_buffer_texture = 0;
        {
            // NOTE: the back buffer has index 0
            UINT buffer = 0;
            REFIID riid = __uuidof(ID3D11Texture2D);
            HRESULT result =
                swap_chain->GetBuffer(
                    buffer,
                    riid,
                    (void**)&back_buffer_texture
                    );
            if( FAILED(result) )
            {
                Platform::log_string("failed to get back buffer texture");
                return 0;
            }
        }
        assert( back_buffer_texture != 0 );

        {
            ID3D11Resource *resource = back_buffer_texture;
            // NOTE: 0 indicates that we want a view with the same format as the buffer
            D3D11_RENDER_TARGET_VIEW_DESC* render_target_view_description = 0;
            HRESULT result =
                d3d_device->CreateRenderTargetView(
                    resource,
                    render_target_view_description,
                    &render_target_view
                    );
            if( FAILED(result) )
            {
                Platform::log_string("failed to get render target view on back buffer");
                return 0 ;
            }
        }

        back_buffer_texture->Release();

    }
    assert(render_target_view);

    {
        uint const num_views = 1;
        ID3D11RenderTargetView* render_target_views[num_views] = {render_target_view};
        ID3D11DepthStencilView* depth_stencil_view = 0;
        d3d_device_context->OMSetRenderTargets(
            num_views,
            render_target_views,
            depth_stencil_view
            );
    }

    {
        uint const num_viewports = 1;
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = viewport_x_dimension_screen;
        viewport.Height = viewport_y_dimension_screen;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        D3D11_VIEWPORT viewports[num_viewports] = {viewport};
        d3d_device_context->RSSetViewports(num_viewports, viewports);
    }

    int const num_marker_colors = 4;
    Vec3::Vec3 marker_colors[num_marker_colors] = {};
    Vec3::set(1,0,0, &marker_colors[0]);
    Vec3::set(0,1,0, &marker_colors[1]);
    Vec3::set(0,0,1, &marker_colors[2]);
    Vec3::set(1,1,0, &marker_colors[3]);
    
    

    ID3D11Buffer* marker_vertex_buffer = 0;
    {
        bool succeeded =
            create_marker_vertex_buffer(
                d3d_device,
                &marker_vertex_buffer
                );
        if(!succeeded)
        {
            Platform::log_string("failed to create marker vertex buffer");
            return 0;
        }
    }
    assert( marker_vertex_buffer != 0 );    

    ID3D11Buffer* marker_index_buffer = 0;
    {
        bool succeeded =
            create_marker_index_buffer(
                d3d_device,
                &marker_index_buffer
                );
        if(!succeeded)
        {
            Platform::log_string("failed to create marker index buffer");
            return 0 ;
        }
    }
    assert( marker_index_buffer != 0 );        
    
    ID3D11Buffer* circle_vertex_buffer = 0;
    {
        bool succeeded =
            create_circle_vertex_buffer(
                d3d_device,
                &circle_vertex_buffer
                );
        if(!succeeded)
        {
            Platform::log_string("failed to create circle vertex buffer");
            return 0 ;
        }
    }
    assert( circle_vertex_buffer != 0 );

    ID3D11Buffer* curve_vertex_buffer = 0;
    {
        uint const num_vertices = num_curve_slices;
        bool const success = 
            create_curve_vertex_buffer(
                num_vertices,
                d3d_device,
                &curve_vertex_buffer
                );
        if(!success)
        {
            Platform::log_string("failed to curve vertex buffer");
            return 0 ;
        }
    }
    assert( curve_vertex_buffer != 0 );
    
    
    ID3D11Buffer* circle_index_buffer = 0;
    {
        bool succeeded =
            create_circle_index_buffer(
                d3d_device,
                &circle_index_buffer
                );
        if(!succeeded)
        {
            Platform::log_string("failed to create circle index buffer");
            return 0;
        }
    }
    assert( circle_index_buffer != 0 );    
    

    ID3D11InputLayout* dynamic_vertex_input_layout = 0;
    ID3D11InputLayout* circle_vertex_input_layout = 0;
    ID3D11InputLayout* curve_vertex_input_layout = 0;
    ID3D11VertexShader* grid_vertex_shader = 0;
    ID3D11VertexShader* colorbar_vertex_shader = 0;
    ID3D11VertexShader* dynamic_vertex_shader = 0;
    ID3D11VertexShader* grid_numbers_vertex_shader = 0;
    ID3D11VertexShader* ttf_grid_numbers_vertex_shader = 0;    
    ID3D11VertexShader* circle_vertex_shader = 0;
    ID3D11VertexShader* plot_vertex_shader = 0;
    
    {

        {
            char* file_name = "grid_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &grid_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }
            
        }

        {
            char* file_name = "colorbar_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &colorbar_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }
            
        }
        
        
        {
            char* file_name = "circle_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &circle_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }

            {
                D3D11_INPUT_ELEMENT_DESC element_descriptions[1] = {};
                        
                element_descriptions[0].SemanticName = "POSITION";
                element_descriptions[0].SemanticIndex = 0; // NOTE: not relevant
                element_descriptions[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                element_descriptions[0].InputSlot = 0;
                element_descriptions[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
                element_descriptions[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                element_descriptions[0].InstanceDataStepRate = 0;
                
                uint num_elements = ARRAY_LENGTH(element_descriptions);
        
                HRESULT result = d3d_device->CreateInputLayout(
                    element_descriptions,
                    num_elements,
                    byte_code,
                    byte_code_size,
                    &circle_vertex_input_layout
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to create circle vertex input layout");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    __debugbreak();
                    return 0;
                }
                
            }
            
            Platform::free_file_memory(byte_code);
            
        }

        {
            char* file_name = "plot_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &plot_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }

            {
                D3D11_INPUT_ELEMENT_DESC element_descriptions[1] = {};
                        
                element_descriptions[0].SemanticName = "POSITION";
                element_descriptions[0].SemanticIndex = 0; // NOTE: not relevant
                element_descriptions[0].Format = DXGI_FORMAT_R32_FLOAT;
                element_descriptions[0].InputSlot = 0;
                element_descriptions[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
                element_descriptions[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                element_descriptions[0].InstanceDataStepRate = 0;
                
                uint num_elements = ARRAY_LENGTH(element_descriptions);
        
                HRESULT result = d3d_device->CreateInputLayout(
                    element_descriptions,
                    num_elements,
                    byte_code,
                    byte_code_size,
                    &curve_vertex_input_layout
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to create curve vertex input layout");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    __debugbreak();
                    return 0;
                }
                
            }
            

            Platform::free_file_memory(byte_code);
            
        }        
        
        {
            char* file_name = "grid_font_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &grid_numbers_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }
            
            Platform::free_file_memory(byte_code);
            
        }        

        {
            char* file_name = "ttf_font_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &ttf_grid_numbers_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
                
            }
            
            Platform::free_file_memory(byte_code);
            
        }                
        
        {
            char* file_name = "solid_dynamic_vs.cso";
            void* byte_code = 0;
            size_t byte_code_size = 0;
            {
                Platform::ReadFileResult result = Platform::read_file(file_name);
                if(result.contents == 0)
                {
                    Platform::log_string("failed to load vertex shader file ");
                    Platform::log_string(file_name);
                    Platform::log_string("\n");
                    return 0;
                }
                byte_code = result.contents;
                byte_code_size = result.contents_size;
            }
            assert(byte_code != 0);
            assert(byte_code_size != 0);

            {
                // STUDY: what is this?
                ID3D11ClassLinkage* class_linkage = 0;
        
                HRESULT result = d3d_device->CreateVertexShader(
                    byte_code,
                    byte_code_size,
                    class_linkage,
                    &dynamic_vertex_shader
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to compile vertex shader");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }            
            }

            {
                D3D11_INPUT_ELEMENT_DESC element_descriptions[1] = {};

                element_descriptions[0].SemanticName = "POSITION";
                element_descriptions[0].SemanticIndex = 0; // NOTE: not relevant
                element_descriptions[0].Format = DXGI_FORMAT_R32G32_FLOAT;
                element_descriptions[0].InputSlot = 0;
                element_descriptions[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
                element_descriptions[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                element_descriptions[0].InstanceDataStepRate = 0;            
            
                uint num_elements = ARRAY_LENGTH(element_descriptions);
        
                HRESULT result = d3d_device->CreateInputLayout(
                    element_descriptions,
                    num_elements,
                    byte_code,
                    byte_code_size,
                    &dynamic_vertex_input_layout
                    );

                if( FAILED(result) )
                {
                    Platform::log_string("failed to create dynamic vertex input layout");
                    Platform::log_string(" (");
                    Platform::log_string(file_name);
                    Platform::log_string(")");
                    Platform::log_string("\n");
                    Platform::free_file_memory(byte_code);
                    return 0;
                }
            }
            
            Platform::free_file_memory(byte_code);            
            
        }
        
    }
    assert(dynamic_vertex_shader != 0);
    assert(dynamic_vertex_input_layout != 0);

    ID3D11PixelShader* solid_pixel_shader = 0;
    {

        char* file_name = "solid_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &solid_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile solid pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(solid_pixel_shader != 0);

    ID3D11PixelShader* colorbar_magnitude_pixel_shader = 0;
    {

        char* file_name = "colorbar_magnitude_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &colorbar_magnitude_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(colorbar_magnitude_pixel_shader != 0);    

    ID3D11PixelShader* colorbar_colorwheel_pixel_shader = 0;
    {

        char* file_name = "colorbar_colorwheel_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &colorbar_colorwheel_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(colorbar_colorwheel_pixel_shader != 0);       
    
    ID3D11PixelShader* font_pixel_shader = 0;
    {

        char* file_name = "grid_font_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &font_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile font pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(font_pixel_shader != 0);

    ID3D11PixelShader* ttf_font_pixel_shader = 0;
    {

        char* file_name = "ttf_font_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &ttf_font_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile ttf font pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(ttf_font_pixel_shader != 0);    
    
    ID3D11PixelShader* density_pixel_shader = 0;
    {

        char* file_name = "density_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &density_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile density pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(density_pixel_shader != 0);    

    ID3D11PixelShader* domain_coloring_pixel_shader = 0;
    {

        char* file_name = "domain_coloring_ps.cso";
        void* byte_code = 0;
        size_t byte_code_size = 0;
        {
            Platform::ReadFileResult result = Platform::read_file(file_name);
            if(result.contents == 0)
            {
                Platform::log_string("failed to read pixel shader file ");
                Platform::log_string(file_name);
                Platform::log_string("\n");
				return 0;
            }
            byte_code = result.contents;
            byte_code_size = result.contents_size;
        }
        assert(byte_code != 0);
        assert(byte_code_size != 0);

        // STUDY: what is this?
        ID3D11ClassLinkage* class_linkage = 0;
        
        HRESULT result = d3d_device->CreatePixelShader(
            byte_code,
            byte_code_size,
            class_linkage,
            &domain_coloring_pixel_shader
            );

        Platform::free_file_memory(byte_code);        
        
        if( FAILED(result) )
        {
            Platform::log_string("failed to compile domain_coloring pixel shader");
            Platform::log_string("(");
            Platform::log_string(file_name);
            Platform::log_string(")");
            Platform::log_string("\n");
            return 0;
        }        
        
    }
    assert(domain_coloring_pixel_shader != 0);    
    
    
    ID3D11Buffer* dynamic_constant_buffer = 0;
    {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(DynamicConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(MatrixConstants)
        description.StructureByteStride = 0;
        
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
        
        HRESULT result = d3d_device->CreateBuffer(
            &description,
            initial_data,
            &dynamic_constant_buffer
            );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create dynamic constant buffer");
            return 0;
        }
    }
    assert( dynamic_constant_buffer != 0 );    

    ID3D11Buffer* plot_constant_buffer = 0;
    {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(PlotConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(MatrixConstants)
        description.StructureByteStride = 0;
        
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
        
        HRESULT result = d3d_device->CreateBuffer(
            &description,
            initial_data,
            &plot_constant_buffer
            );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create plot constant buffer");
            return 0;
        }
    }
    assert( plot_constant_buffer != 0 );    
    
    ID3D11Buffer* layout_constant_buffer = 0;
    {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(WidgetLayoutConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(MatrixConstants)
        description.StructureByteStride = 0;
        
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
        
        HRESULT result = d3d_device->CreateBuffer(
            &description,
            initial_data,
            &layout_constant_buffer
            );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create layout constant buffer");
            return 0;
        }
    }
    assert( layout_constant_buffer != 0 );    

    ID3D11Buffer* grid_constant_buffer = 0;
    {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(Grid::GridLinesShaderConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(MatrixConstants)
        description.StructureByteStride = 0;
        
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
        
        HRESULT result = d3d_device->CreateBuffer(
            &description,
            initial_data,
            &grid_constant_buffer
            );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create grid constant buffer");
            return 0;
        }
        
    }
    assert( grid_constant_buffer != 0 );
    
    
    ID3D11Buffer* grid_numbers_constant_buffer = 0;
    {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = sizeof(Grid::TextShaderConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        description.MiscFlags = 0;
        // STUDY: why not set this to the size of the buffer element? sizeof(MatrixConstants)
        description.StructureByteStride = 0;
        
        D3D11_SUBRESOURCE_DATA* initial_data = 0;
        
        HRESULT result = d3d_device->CreateBuffer(
            &description,
            initial_data,
            &grid_numbers_constant_buffer
            );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create grid text constant buffer");
            return 0;
        }
        
    }
    assert( grid_numbers_constant_buffer != 0 );
    
    ID3D11Texture2D* font_texture = 0;
    {

        int const width = Grid::FONT_TEXTURE_X_DIMENSION_SCREEN;
        int const height = Grid::FONT_TEXTURE_Y_DIMENSION_SCREEN;
        
        D3D11_TEXTURE2D_DESC description = {};
        {
            description.Width = width;
            description.Height = height;
            description.MipLevels = 1;
            description.ArraySize = 1;
            // OPTIMIZE: use a format that takes less space, we only need 1 bit per pixel
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.SampleDesc.Quality = 0;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            description.CPUAccessFlags = 0;
            description.MiscFlags = 0;
        }

        uint32 pixels[width*height] = {};
        Grid::font_texture(pixels);

        D3D11_SUBRESOURCE_DATA initial_data = {};
        {
            initial_data.pSysMem = pixels;
            initial_data.SysMemPitch = sizeof(uint32)*width;
            // NOTE: has no meaning for 2d textures and is ignored
            initial_data.SysMemSlicePitch = 0;
        }
        
        HRESULT const result =
            d3d_device->CreateTexture2D(
                &description,
                &initial_data, 
                &font_texture
                );

        if(FAILED(result))
        {
            Platform::log_line_string("failed to create font texture");
            return 0;
        }
        
    }
    assert(font_texture != 0);

    ID3D11Texture2D* ttf_font_texture = 0;
    {

        int const width = 8;
        int const height = 8;
        
        D3D11_TEXTURE2D_DESC description = {};
        {
            description.Width = width;
            description.Height = height;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.SampleDesc.Quality = 0;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            description.CPUAccessFlags = 0;
            description.MiscFlags = 0;
        }

        uint32 w = 0x000000ff;
        uint32 b = 0x00000000;
        uint32 pixels[width*height] = 
            {
                
                b,w,b,w, b,w,b,w, 
                w,w,w,b, w,b,w,b, 
                b,w,b,w, b,w,b,w, 
                w,w,w,b, w,b,w,b, 
                
                w,w,w,w, w,w,b,w, 
                b,b,b,b, w,w,b,w, 
                w,w,w,w, w,w,b,w, 
                w,w,w,w, w,w,b,w, 

                
            };
        
        D3D11_SUBRESOURCE_DATA initial_data = {};
        {
            initial_data.pSysMem = pixels;
            initial_data.SysMemPitch = sizeof(uint32)*width;
            // NOTE: has no meaning for 2d textures and is ignored
            initial_data.SysMemSlicePitch = 0;
        }
        
        HRESULT const result =
            d3d_device->CreateTexture2D(
                &description,
                &initial_data, 
                &ttf_font_texture
                );

        if(FAILED(result))
        {
            Platform::log_line_string("failed to create ttf font texture");
            return 0;
        }
        
    }
    assert(ttf_font_texture != 0);
    
    ID3D11ShaderResourceView* font_texture_srv = 0;
    {

        ID3D11Resource *const resource = font_texture;
        D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
        {
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            description.Texture2D.MostDetailedMip = 0;
            description.Texture2D.MipLevels = 1;
        }
        
        HRESULT const result =
            d3d_device->CreateShaderResourceView(
                resource,
                &description,
                &font_texture_srv
                );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create font texture shader resource view");
            return 0;
        }
        
    }
    assert(font_texture_srv != 0);

    ID3D11ShaderResourceView* ttf_font_texture_srv = 0;
    {

        ID3D11Resource *const resource = ttf_font_texture;
        D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
        {
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            description.Texture2D.MostDetailedMip = 0;
            description.Texture2D.MipLevels = 1;
        }
        
        HRESULT const result =
            d3d_device->CreateShaderResourceView(
                resource,
                &description,
                &ttf_font_texture_srv
                );
        
        if(FAILED(result))
        {
            Platform::log_line_string("failed to create ttf_font texture shader resource view");
            return 0;
        }
        
    }
    assert(ttf_font_texture_srv != 0);    
    
    ID3D11SamplerState* font_sampler_state = 0;
    {

        D3D11_SAMPLER_DESC description = {};
        {
            description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.MipLODBias = 0.0f;
            description.MaxAnisotropy = 1;
            description.ComparisonFunc = D3D11_COMPARISON_EQUAL;
            description.BorderColor[4] = {};
            description.MinLOD = 0;
            description.MaxLOD = 0;
        }
        
        HRESULT const result =
            d3d_device->CreateSamplerState(
                &description,
                &font_sampler_state
                );

        if(FAILED(result))
        {
            Platform::log_line_string("failed to create font sampler state");
            return 0;
        }
        
    }
    assert(font_sampler_state != 0);

    ID3D11SamplerState* ttf_font_sampler_state = 0;
    {

        D3D11_SAMPLER_DESC description = {};
        {
            description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            description.MipLODBias = 0.0f;
            description.MaxAnisotropy = 1;
            description.ComparisonFunc = D3D11_COMPARISON_EQUAL;
            description.BorderColor[4] = {};
            description.MinLOD = 0;
            description.MaxLOD = 0;
        }
        
        HRESULT const result =
            d3d_device->CreateSamplerState(
                &description,
                &ttf_font_sampler_state
                );

        if(FAILED(result))
        {
            Platform::log_line_string("failed to create ttf_font sampler state");
            return 0;
        }
        
    }
    assert(ttf_font_sampler_state != 0);    
    
    float time = 0.0f;
    Platform::InputState input_state = {};
    input_state.mouse_input_enabled = mouse_input_initially_enabled;    

    Parameters parameters = {};
    Complex::set_polar(0.25f, PI_FLOAT*0.25f, &parameters.parameter.zero[0]);
    Complex::set_polar(0.75f, PI_FLOAT*0.5f, &parameters.parameter.zero[1]);
    Complex::set_polar(0.25f, PI_FLOAT*0.1f, &parameters.parameter.pole[0]);
    Complex::set_polar(0.75f, PI_FLOAT*0.75f, &parameters.parameter.pole[1]);
    
    int selected_parameter_idx = -1;
    int side_idx = -1;
    int widget_zoom = 0;

    float plotviewport_center_x_plotdata[2] = {0.5f, 0.5f};
    float plotviewport_center_y_plotdata[2] = {0.75f, 0.0f};
    int x_zoom_level_plotdata[2] = {};
    int y_zoom_level_plotdata[2] = {};
    float const plotviewport_unzoomed_x_dimension_plotdata[2] = {1.05f, 1.15f};
    float const plotviewport_unzoomed_y_dimension_plotdata[2] = {1.6f, 1.05f};
    int dragged_plot = -1;
    float plot_drag_start_x_viewport = 0;
    float plot_drag_start_y_viewport = 0;
    
    while( true )
    {
        Platform::FrameContext frame_context = {};
        Platform::frame_start(&frame_context);
        
        Platform::InputContext input_context = {};
        Platform::get_input(&input_context, &input_state);

        int cursor_x_position_window;
        int cursor_y_position_window;
        if(
            !Platform::try_get_system_cursor_local_position(
                &cursor_x_position_window,
                &cursor_y_position_window
                )
            )
        {
            using namespace Log;
            string("Failed to get the system cursor position");
            newline();
        }

        float const cursor_x_position_viewport =
            (float(cursor_x_position_window) - float(viewport_x_dimension_screen)*0.5f)/
            (float(viewport_x_dimension_screen)*0.5f);
        
        float const cursor_y_position_viewport =
            (float(viewport_y_dimension_screen)*0.5f - float(cursor_y_position_window))/
            (0.5f*float(viewport_y_dimension_screen));

        {
            Platform::ButtonState* quit_button = &input_state.quit;
            bool quit_button_pushed = quit_button->changed_state && quit_button->pressed;
            bool should_exit = input_context.quit || quit_button_pushed;
            if( should_exit )
                break;
        }
        
        // Get work start time in counts
#if IIR4_WIDGET_PERFORMANCE_SPAM_LEVEL > 0        
        Platform::TimeCount work_start_counts = Platform::time_get_count();
#endif        


        int hovered_plotviewport = -1;
        for(int i=0; i<2; i++)
        {
            float const plotviewport_min_y_screen = (1.0f - plotviewport_max_y_viewport[i]) * viewport_y_unit_screen;
            float const plotviewport_max_y_screen = (1.0f - plotviewport_min_y_viewport[i]) * viewport_y_unit_screen;
        
            bool const hovered =
                Geometry2::rectangle_point_intersect(
                    plotviewport_min_x_viewport,
                    plotviewport_max_x_viewport,
                    plotviewport_min_y_viewport[i],
                    plotviewport_max_y_viewport[i],
                    cursor_x_position_viewport,
                    cursor_y_position_viewport
                    );
            if(hovered)
            {
                hovered_plotviewport = i;
                break;
            }
        }

        int const previously_dragged_plot = dragged_plot;
        if(dragged_plot != -1)
        {

            if(got_released(&input_state.mouse_left))
            {
                dragged_plot = -1;
            }
            
        }
        else if(hovered_plotviewport != -1)
        {

            if(got_pressed(&input_state.mouse_left))
            {
                assert(dragged_plot == -1);
                dragged_plot = hovered_plotviewport;
            }

        }

        bool const zooming = (dragged_plot == -1) && input_state.mouse_wheel_delta != 0;
        int const zoomed_plot = zooming ? hovered_plotviewport : -1;
        bool const zooming_widget = (hovered_plotviewport == -1) && zooming;

        if(zooming_widget)
        {
            widget_zoom = Numerics::minimum(0, widget_zoom + Numerics::sign(input_state.mouse_wheel_delta));
        }

        float const prev_x_zoom_plotdata[2] =
            {
                x_zoom_level_plotdata[0]*zoom_step_size,
                x_zoom_level_plotdata[1]*zoom_step_size,
            };
        
        float const prev_y_zoom_plotdata[2] =
            {
                y_zoom_level_plotdata[0]*zoom_step_size,
                y_zoom_level_plotdata[1]*zoom_step_size,
            };

        float x_zoom_plotdata[2] = {prev_x_zoom_plotdata[0], prev_x_zoom_plotdata[1]};
        float y_zoom_plotdata[2] = {prev_y_zoom_plotdata[0], prev_y_zoom_plotdata[1]};
        
        if(zoomed_plot != -1)
        {

            assert(hovered_plotviewport == 0 || hovered_plotviewport == 1);
            assert(zoomed_plot == 0 || zoomed_plot == 1);

            x_zoom_level_plotdata[zoomed_plot] += Numerics::sign(input_state.mouse_wheel_delta);
            y_zoom_level_plotdata[zoomed_plot] += Numerics::sign(input_state.mouse_wheel_delta);

            x_zoom_plotdata[zoomed_plot] = x_zoom_level_plotdata[zoomed_plot]*zoom_step_size;
            y_zoom_plotdata[zoomed_plot] = y_zoom_level_plotdata[zoomed_plot]*zoom_step_size;
            
            // NOTE:
            // We require that the cursor stays fixeds in the 'plotdata' coordinate system.
            // The same is assumed true for the cursor position in the fixed 'plot' coordinate system.
            //
            // In other words we have the constraint
            // new_cursor_position_plotdata = old_cursor_position_plotdata

            // The transformation from plot coordinates to plotdata coordinates has the form
            // x_plotdata = center_x_plotdata + x_plot * plot_unit_plotdata;

            // Thus the above constratint becomes
            // new_center_x_plotdata + cursor_x_position_plot*new_plot_unit_plotdata =
            // old_center_x_plotdata + cursor_x_position_plot*old_plot_unit_plotdata

            // Solving this, we get
            // new_center_x_plotdata =
            // old_center_x_plotdata +
            // cursor_x_position_plot*(old_plot_unit_plotdata - new_plot_unit_plotdata)

            float const plotviewport_center_y_viewport =
                (
                    plotviewport_min_y_viewport[zoomed_plot] +
                    plotviewport_max_y_viewport[zoomed_plot]
                    )*0.5f;
            
            float const cursor_x_position_plot =
                cursor_x_position_viewport - plotviewport_center_x_viewport;
            
            float const cursor_y_position_plot =
                cursor_y_position_viewport - plotviewport_center_y_viewport;
            
            float const old_plot_x_unit_plotdata =
                plotviewport_unzoomed_x_dimension_plotdata[zoomed_plot] * Numerics::power(float(grid_base), -prev_x_zoom_plotdata[zoomed_plot]) / plotviewport_x_dimension_viewport;
            float const new_plot_x_unit_plotdata =
                plotviewport_unzoomed_x_dimension_plotdata[zoomed_plot] * Numerics::power(float(grid_base), -x_zoom_plotdata[zoomed_plot]) / plotviewport_x_dimension_viewport;
            float const old_plot_y_unit_plotdata =
                plotviewport_unzoomed_y_dimension_plotdata[zoomed_plot] * Numerics::power(float(grid_base), -prev_y_zoom_plotdata[zoomed_plot]) / plotviewport_y_dimension_viewport;
            float const new_plot_y_unit_plotdata =
                plotviewport_unzoomed_y_dimension_plotdata[zoomed_plot] * Numerics::power(float(grid_base), -y_zoom_plotdata[zoomed_plot]) / plotviewport_y_dimension_viewport;
            
            plotviewport_center_x_plotdata[zoomed_plot] +=
                cursor_x_position_plot*(old_plot_x_unit_plotdata - new_plot_x_unit_plotdata);

            plotviewport_center_y_plotdata[zoomed_plot] +=
                cursor_y_position_plot*(old_plot_y_unit_plotdata - new_plot_y_unit_plotdata);
            
            
        }
        
        if(dragged_plot != -1 && dragged_plot != previously_dragged_plot)
        {
			assert(previously_dragged_plot == -1);
            assert(dragged_plot == 0 || dragged_plot == 1);
            plot_drag_start_x_viewport = cursor_x_position_viewport;
            plot_drag_start_y_viewport = cursor_y_position_viewport;
        }

        float const viewport_x_unit_plotdata[2] =
            {
                plotviewport_unzoomed_x_dimension_plotdata[0] * Numerics::power(float(grid_base), -x_zoom_plotdata[0]) / plotviewport_x_dimension_viewport,
                plotviewport_unzoomed_x_dimension_plotdata[1] * Numerics::power(float(grid_base), -x_zoom_plotdata[1]) / plotviewport_x_dimension_viewport,
            };
        
        float const viewport_y_unit_plotdata[2] =
            {
                plotviewport_unzoomed_y_dimension_plotdata[0] * Numerics::power(float(grid_base), -y_zoom_plotdata[0]) / plotviewport_y_dimension_viewport,
                plotviewport_unzoomed_y_dimension_plotdata[1] * Numerics::power(float(grid_base), -y_zoom_plotdata[1]) / plotviewport_y_dimension_viewport,
            };
        
        float const drag_offset_x_plotdata[2] =
        {
            (cursor_x_position_viewport - plot_drag_start_x_viewport) * viewport_x_unit_plotdata[0],
            (cursor_x_position_viewport - plot_drag_start_x_viewport) * viewport_x_unit_plotdata[1],
        };
        
        float const drag_offset_y_plotdata[2] =
        {
            (cursor_y_position_viewport - plot_drag_start_y_viewport) * viewport_y_unit_plotdata[0],
            (cursor_y_position_viewport - plot_drag_start_y_viewport) * viewport_y_unit_plotdata[1],
        };
        
        if(dragged_plot == -1 && dragged_plot != previously_dragged_plot)
        {
            assert(previously_dragged_plot == 0 || previously_dragged_plot == 1);
            plotviewport_center_x_plotdata[previously_dragged_plot] -= drag_offset_x_plotdata[previously_dragged_plot];
            plotviewport_center_y_plotdata[previously_dragged_plot] -= drag_offset_y_plotdata[previously_dragged_plot];
        }

        float const widgetviewport_x_unit_viewport =
            float(widgetviewport_x_dimension_screen)/viewport_x_dimension_screen;

        float const widgetviewport_y_unit_viewport =
            float(widgetviewport_y_dimension_screen)/viewport_y_dimension_screen;

        float const widgetdata_x_unit_widgetviewport = Numerics::power(1.1f, float(widget_zoom));
        float const widgetdata_y_unit_widgetviewport = Numerics::power(1.1f, float(widget_zoom));
        
        const float widgetdata_x_unit_viewport =
            widgetdata_x_unit_widgetviewport * widgetviewport_x_unit_viewport;
        
        const float widgetdata_y_unit_viewport =
            widgetdata_y_unit_widgetviewport * widgetviewport_y_unit_viewport;

        float const cursor_x_position_widgetdata =
            (cursor_x_position_viewport - widgetviewport_center_x_viewport)/widgetviewport_x_unit_viewport;
        

        float const cursor_y_position_top_widgetdata =
            (cursor_y_position_viewport - top_widgetviewport_center_y_viewport)/widgetdata_y_unit_viewport;
        
        float const cursor_y_position_bottom_widgetdata =
            (cursor_y_position_viewport - bottom_widgetviewport_center_y_viewport)/widgetdata_y_unit_viewport;
        
        if(hovered_plotviewport == -1 && !input_state.mouse_input_enabled)
        {
            
            
            bool const parameter_selection_toggled =
                input_state.mouse_left.pressed == false && input_state.mouse_left.changed_state == true;
            
            if(parameter_selection_toggled)
            {
                
                bool const no_previous_selection = selected_parameter_idx == -1;
                if(no_previous_selection)
                {

                    float closest_distance_top_sq = POSITIVE_INFINITY_FLOAT;
                    int parameter_top_idx = 0;
                    for(int parameter_idx=0; parameter_idx < ARRAY_LENGTH(parameters.parameters); parameter_idx++)
                    {
                        Complex::C const*const parameter = &parameters.parameters[parameter_idx];
                        float const distance_sq =
                            Complex::distance_squared(
                                cursor_x_position_widgetdata,
                                cursor_y_position_top_widgetdata,
                                parameter
                                );
                        Complex::C conjugate_parameter;
                        Complex::conjugate(parameter, &conjugate_parameter);
                        float const distance_conjugate_sq =
                            Complex::distance_squared(
                                cursor_x_position_widgetdata,
                                cursor_y_position_top_widgetdata,
                                &conjugate_parameter
                                );
                        float const min_distance_sq = Numerics::minimum(distance_sq, distance_conjugate_sq);
                        if(min_distance_sq < closest_distance_top_sq)
                        {
                            parameter_top_idx = parameter_idx;
                            closest_distance_top_sq = min_distance_sq;
                        }
                    }

                    float closest_distance_bottom_sq = POSITIVE_INFINITY_FLOAT;
                    int parameter_bottom_idx = 0;                    
                    for(int parameter_idx=0; parameter_idx < ARRAY_LENGTH(parameters.parameters); parameter_idx++)
                    {
                        Complex::C const*const parameter = &parameters.parameters[parameter_idx];
                        float const distance_sq =
                            Complex::distance_squared(
                                cursor_x_position_widgetdata,
                                cursor_y_position_bottom_widgetdata, parameter
                                );
                        Complex::C conjugate_parameter;
                        Complex::conjugate(parameter, &conjugate_parameter);
                        float const distance_conjugate_sq =
                            Complex::distance_squared(
                                cursor_x_position_widgetdata,
                                cursor_y_position_bottom_widgetdata,
                                &conjugate_parameter
                                );
                        float const min_distance_sq = Numerics::minimum(distance_sq, distance_conjugate_sq);
                        if(min_distance_sq < closest_distance_bottom_sq)
                        {
                            parameter_bottom_idx = parameter_idx;
                            closest_distance_bottom_sq = min_distance_sq;
                        }
                    }


                    side_idx = closest_distance_top_sq < closest_distance_bottom_sq ? +1 : -1;
                    if(side_idx == -1)
                        selected_parameter_idx = parameter_bottom_idx;
                    else if(side_idx == +1)
                        selected_parameter_idx = parameter_top_idx;
                    else
                        assert(false);
                    
                }
                else
                {
                    selected_parameter_idx = -1;
                }
                
            }

        }
        
        // NOTE: animate parameters or else read them from mouse input
        if(input_state.mouse_input_enabled)
        {

            // NOTE: random animation I made up

            float const s = 0.4f;
            parameters.parameter.zero[0] = {1.0f*sinf(time*s), -0.45f*cosf(time*s)};
            parameters.parameter.zero[1] = {0.252f*sinf(time*s*1.2f), 0.25f*sinf(time*s/2)};

            parameters.parameter.pole[0] = {0.25f*sinf(-2.3f*time*s), -0.25f*cosf(time*s*0.154f)};
            parameters.parameter.pole[1] = {0.35f*sinf(time*s), 0.25f*cosf(time*s*0.5f)};            
            
        }
        else
        {

            parameters.parameters[selected_parameter_idx] =
                {
                    cursor_x_position_widgetdata,
                    side_idx == -1 ? cursor_y_position_bottom_widgetdata : cursor_y_position_top_widgetdata,
                };
            
        }

        float const normalization_factor = normalization_constant_highpass(&parameters);
        
        // NOTE: set scissor rectangle to entire viewport
        {
            D3D11_RECT rectangles[1] = {};
            rectangles[0].left = 0;
            rectangles[0].top = 0;
            rectangles[0].right = viewport_x_dimension_screen;
            rectangles[0].bottom = viewport_y_dimension_screen;
            uint const num_rectangles = ARRAY_LENGTH(rectangles);
            d3d_device_context->RSSetScissorRects(
                num_rectangles,
                rectangles
                );
        }
        
        {
            FLOAT* blend_factor = 0;
            UINT sample_mask = 0xffffffff;
            d3d_device_context->OMSetBlendState(blend_state, blend_factor, sample_mask);
        }
        
        {
            FLOAT const clear_color[4] = {0.0f, 0.2f, 0.3f, 0.0};
            d3d_device_context->ClearRenderTargetView(render_target_view, clear_color);
        }
        
        uint dynamic_cbuffer_slot = 0;
        
        {
            uint const num_buffers = 1;
            ID3D11Buffer* buffers[num_buffers] = {dynamic_constant_buffer};
            uint start_slot = dynamic_cbuffer_slot;
            d3d_device_context->PSSetConstantBuffers(
                start_slot,
                num_buffers,
                buffers
                );            
            
        }
        
        
        {
            uint const num_buffers = 2;
            ID3D11Buffer* buffers[num_buffers] = {dynamic_constant_buffer, layout_constant_buffer};
            d3d_device_context->VSSetConstantBuffers(
                dynamic_cbuffer_slot,
                num_buffers,
                buffers
                );
        }        
        
        d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        {
            bool const success = 
                update_parameters(
                    d3d_device_context,
                    dynamic_constant_buffer,
                    &parameters,
                    normalization_factor
                    );
            assert(success);
        }        
        
        
        // NOTE: update widget layout constants
        {

            WidgetLayoutConstants layout_constants = {};
            layout_constants.center_x_position_viewport = widgetviewport_center_x_viewport;
            layout_constants.center_y_position_viewport = top_widgetviewport_center_y_viewport;
            layout_constants.data_x_unit_viewport = widgetdata_x_unit_viewport;
            layout_constants.data_y_unit_viewport = widgetdata_y_unit_viewport;

            bool const success =
                update_layout_constants(
                    d3d_device_context,
                    layout_constant_buffer,
                    &layout_constants
                    );
            assert(success);                    
                    
        }

        // NOTE: draw magnitude density
        {
            // NOTE: set the density pixel shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->PSSetShader(
                    density_pixel_shader,
                    class_instances,
                    num_class_instances
                    );            
            }        
        
            draw_circle(
                d3d_device_context,
                circle_vertex_buffer,
                circle_index_buffer,
                circle_vertex_input_layout,
                circle_vertex_shader
                );
        }
        
        draw_markers(
            d3d_device_context,
            dynamic_vertex_input_layout,
            dynamic_vertex_shader,
            marker_vertex_buffer,
            marker_index_buffer,
            solid_pixel_shader
            
            );

        // NOTE: update widget layout constants
        {

            WidgetLayoutConstants layout_constants = {};
            layout_constants.center_x_position_viewport = widgetviewport_center_x_viewport;
            layout_constants.center_y_position_viewport = bottom_widgetviewport_center_y_viewport;
            layout_constants.data_x_unit_viewport = widgetdata_x_unit_viewport;
            layout_constants.data_y_unit_viewport = widgetdata_y_unit_viewport;

            bool const success = update_layout_constants(
                d3d_device_context,
                layout_constant_buffer,
                &layout_constants
                );
                
            assert(success);
                    
                    
        }

        // NOTE: draw domain coloring
        {
            // NOTE: set the density pixel shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->PSSetShader(
                    domain_coloring_pixel_shader,
                    class_instances,
                    num_class_instances
                    );            
            }        
        
            draw_circle(
                d3d_device_context,
                circle_vertex_buffer,
                circle_index_buffer,
                circle_vertex_input_layout,
                circle_vertex_shader
                );
        }

        draw_markers(
            d3d_device_context,
            dynamic_vertex_input_layout,
            dynamic_vertex_shader,
            marker_vertex_buffer,
            marker_index_buffer,
            solid_pixel_shader
            );
        
        d3d_device_context->IASetInputLayout(dynamic_vertex_input_layout);

        Grid::Transform plot_x_transform[2];
        Grid::Transform plot_y_transform[2];

        for(int plot_idx=0; plot_idx<2; plot_idx++)
        {
            
            float const smallest_visible_horizontal_level_spacing_screen = 10.0f;
            float const smallest_visible_vertical_level_spacing_screen = 15.0f;
            
            float const smallest_visible_horizontal_level_spacing_viewport =
                smallest_visible_horizontal_level_spacing_screen*screen_y_unit_viewport;
            float const smallest_visible_vertical_level_spacing_viewport =
                smallest_visible_vertical_level_spacing_screen*screen_x_unit_viewport;
            

            float const plotviewport_dragged_center_x_plotdata =
                (dragged_plot == plot_idx) ?
                plotviewport_center_x_plotdata[plot_idx] - drag_offset_x_plotdata[dragged_plot] :
                plotviewport_center_x_plotdata[plot_idx];
            float const plotviewport_dragged_center_y_plotdata =
                (dragged_plot == plot_idx) ?
                plotviewport_center_y_plotdata[plot_idx] - drag_offset_y_plotdata[dragged_plot] :
                plotviewport_center_y_plotdata[plot_idx];

            using namespace Numerics;

            float const viewport_x_dimension_widgetdata =
                viewport_x_unit_plotdata[plot_idx]*viewport_x_dimension_viewport;
            
            float const viewport_y_dimension_widgetdata =
                power(float(grid_base), -y_zoom_plotdata[plot_idx])*viewport_y_dimension_viewport;

            float const plotviewport_x_dimension_plotdata =
                plotviewport_unzoomed_x_dimension_plotdata[plot_idx] *
                Numerics::power(float(grid_base), -x_zoom_plotdata[plot_idx]);

            float const plotviewport_y_dimension_plotdata =
                plotviewport_unzoomed_y_dimension_plotdata[plot_idx] *
                Numerics::power(float(grid_base), -y_zoom_plotdata[plot_idx]);
            
            plot_x_transform[plot_idx].viewport_min_data =
                plotviewport_dragged_center_x_plotdata - plotviewport_x_dimension_plotdata*0.5f;
            
            plot_x_transform[plot_idx].viewport_max_data =
                plotviewport_dragged_center_x_plotdata + plotviewport_x_dimension_plotdata*0.5f;

            plot_y_transform[plot_idx].viewport_min_data =
                plotviewport_dragged_center_y_plotdata - plotviewport_y_dimension_plotdata*0.5f;
            
            plot_y_transform[plot_idx].viewport_max_data =
                plotviewport_dragged_center_y_plotdata + plotviewport_y_dimension_plotdata*0.5f;

            float const text_end_x_viewport = plotviewport_min_x_viewport;
            float const text_end_y_viewport = plotviewport_min_y_viewport[plot_idx];

            Grid::draw_grid(
                character_spacing_screen,
                grid_base,
                smallest_visible_horizontal_level_spacing_viewport,
                smallest_visible_vertical_level_spacing_viewport,
                plotviewport_min_x_viewport,
                plotviewport_max_x_viewport,
                plotviewport_min_y_viewport[plot_idx],
                plotviewport_max_y_viewport[plot_idx],
                viewport_x_dimension_screen,
                viewport_y_dimension_screen,
                text_end_x_viewport,
                text_end_y_viewport,
                &plot_y_transform[plot_idx],
                &plot_x_transform[plot_idx],
                d3d_device_context,
                grid_vertex_shader,
                solid_pixel_shader,
                grid_constant_buffer,
                grid_numbers_vertex_shader,
                font_pixel_shader,
                font_sampler_state,
                font_texture_srv,
                grid_numbers_constant_buffer
                );

        }
        
        // NOTE: draw ttf font
        if(0)
        {            
            
            // NOTE: set the pixel shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->PSSetShader(
                    ttf_font_pixel_shader,
                    class_instances,
                    num_class_instances
                    );
            }

            // NOTE: set the vertex shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->VSSetShader(
                    ttf_grid_numbers_vertex_shader,
                    class_instances,
                    num_class_instances
                    );
            }            

            // NOTE: set ttf font sampler
            {
            
                uint const start_slot = 1;
                ID3D11SamplerState *const samplers[] = {ttf_font_sampler_state};
                uint num_samplers = ARRAY_LENGTH(samplers); 
            
                d3d_device_context->PSSetSamplers(
                    start_slot,
                    num_samplers,
                    samplers
                    );
            
            }
            
            {

                uint const start_slot = 1;
                ID3D11ShaderResourceView *const shader_resource_views[] = {ttf_font_texture_srv};
                uint const num_views = ARRAY_LENGTH(shader_resource_views);
                
                d3d_device_context->PSSetShaderResources(
                    start_slot,
                    num_views,
                    shader_resource_views
                    );
                
            }
            
            d3d_device_context->IASetInputLayout(0);
            d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            {
                uint const vertex_count = 4;
                uint const start_vertex_location = 0;
                
                d3d_device_context->Draw(
                    vertex_count,
                    start_vertex_location
                    );
            }
            
        }

        // NOTE: draw the plots
        for(int plot_idx=0; plot_idx<2; plot_idx++)
        {            
            
            {
                uint input_slot = 0;
                uint const num_buffers = 1;
                ID3D11Buffer* buffers[num_buffers] = {curve_vertex_buffer};
                uint strides[num_buffers] = {sizeof(float)};
                uint offsets[num_buffers] = {0};
                d3d_device_context->IASetVertexBuffers(
                    input_slot,
                    num_buffers,
                    buffers,
                    strides,
                    offsets
                    );
            }
            
            uint plot_cbuffer_slot = 2;
            {
                uint const num_buffers = 1;
                ID3D11Buffer* buffers[num_buffers] = {plot_constant_buffer};
                d3d_device_context->VSSetConstantBuffers(
                    plot_cbuffer_slot,
                    num_buffers,
                    buffers
                    );
            }

            {

                float const plot_viewport_x_dimension_viewport = plotviewport_x_dimension_viewport;
                float const plot_viewport_y_dimension_viewport = plotviewport_y_dimension_viewport;
                
                float const min_x_plotdata = Numerics::maximum(0.0f, plot_x_transform[plot_idx].viewport_min_data);
                float const max_x_plotdata = Numerics::minimum(1.0f, plot_x_transform[plot_idx].viewport_max_data);
                
                // NOTE: update the curve
                if(plot_idx == 0)
                {
                    // NOTE: frequency response plot

                    float vertices[num_curve_slices];
                    for(int slice_idx=0; slice_idx < num_curve_slices; slice_idx++)
                    {
                        float const t = float(slice_idx)/float(num_curve_segments);
                        float const angle = Numerics::lerp(min_x_plotdata, max_x_plotdata, t) * PI_FLOAT;
                        Complex::C sample_point;
                        Complex::unit_circle_point(angle, &sample_point);

                        float a[2][2];
                        for(int i=0; i<2; i++)
                        {
                            for(int j=0; j<2; j++)
                            {
                                Complex::C const*const p = &parameters.ator_factors[i][j];
                                Complex::C p_conjugate;
                                Complex::conjugate(p, &p_conjugate);
                                a[i][j] =
                                    Complex::distance(&sample_point, p)*Complex::distance(&sample_point, &p_conjugate);
                            }
                        }
                        
                        vertices[slice_idx] =
                            normalization_factor*(a[0][0]*a[0][1])/(a[1][0]*a[1][1]);
                    }
            
                    bool const success =
                        try_upload_curve_vertices(
                            num_curve_slices,
                            vertices,
                            d3d_device_context,
                            curve_vertex_buffer
                            );

                    assert(success);
            
                }
                else if(plot_idx == 1)
                {
                    // NOTE: frequency phase plot

                    float vertices[num_curve_slices];
                    for(int slice_idx=0; slice_idx < num_curve_slices; slice_idx++)
                    {
                        float const t = float(slice_idx)/float(num_curve_segments);
                        float const angle = Numerics::lerp(min_x_plotdata, max_x_plotdata, t) * PI_FLOAT;
                        Complex::C sample_point;
                        Complex::unit_circle_point(angle, &sample_point);

                        Complex::C ator[2];
                        for(int i=0; i<2; i++)
                        {
                            Complex::unit(&ator[i]);
                            for(int j=0; j<2; j++)
                            {
                                Complex::C const*const p = &parameters.ator_factors[i][j];
                                Complex::C p_conjugate;
                                Complex::conjugate(p, &p_conjugate);

                                Complex::C d;
                                Complex::difference(&sample_point, p, &d);
                                
                                Complex::C d_conjugate;
                                Complex::difference(&sample_point, &p_conjugate, &d_conjugate);
                                
                                Complex::multiply(&d, &ator[i]);
                                Complex::multiply(&d_conjugate, &ator[i]);
                                
                            }
                        }

                        Complex::C image;
                        Complex::quotient(&ator[0], &ator[1], &image);
                        float const phase = Complex::phase(&image);
                        
                        vertices[slice_idx] = 0.5f*phase/PI_FLOAT;
                    }
            
                    bool const success =
                        try_upload_curve_vertices(
                            num_curve_slices,
                            vertices,
                            d3d_device_context,
                            curve_vertex_buffer
                            );

                    assert(success);
            
                }

                
                PlotConstants constants;
                float const rectangle_plotdata[4] =
                    {
                        plot_x_transform[plot_idx].viewport_min_data,
                        plot_x_transform[plot_idx].viewport_max_data,
                        plot_y_transform[plot_idx].viewport_min_data,
                        plot_y_transform[plot_idx].viewport_max_data
                    };
                float const rectangle_viewport[4] =
                    {
                        plotviewport_min_x_viewport,
                        plotviewport_max_x_viewport,
                        plotviewport_min_y_viewport[plot_idx],
                        plotviewport_max_y_viewport[plot_idx],
                    };

				memcpy(constants.plotviewport_data, rectangle_plotdata, sizeof(rectangle_plotdata));
				memcpy(constants.plotviewport_viewport, rectangle_viewport, sizeof(rectangle_viewport));
                constants.curve_interval_x_data[0] = min_x_plotdata;
                constants.curve_interval_x_data[1] = max_x_plotdata;
                constants.num_curve_slices = num_curve_slices;
                constants.margin_x_dimension_viewport = plotviewportmargin_x_dimension_viewport;
                
                bool const success = 
                    update_plot_constants(
                        d3d_device_context,
                        plot_constant_buffer,
                        &constants
                        );
                assert(success);
            }

            // NOTE: set scissor rectangle to entire viewport
            {
                D3D11_RECT rectangles[1] = {};
                rectangles[0].left = 0;
                rectangles[0].top = 0;
                rectangles[0].right = viewport_x_dimension_screen;
                rectangles[0].bottom = viewport_y_dimension_screen;
                uint const num_rectangles = ARRAY_LENGTH(rectangles);
                d3d_device_context->RSSetScissorRects(
                    num_rectangles,
                    rectangles
                    );
            }
            
            // NOTE: draw color bars
            {

                // NOTE: set the vertex shader
                {
                    uint num_class_instances = 0;
                    ID3D11ClassInstance** class_instances = 0;
                    d3d_device_context->VSSetShader(
                        colorbar_vertex_shader,
                        class_instances,
                        num_class_instances
                        );
                }
                
                // NOTE: set the pixel shader
                {
                    uint num_class_instances = 0;
                    ID3D11ClassInstance** class_instances = 0;
                    d3d_device_context->PSSetShader(
                        plot_idx == 0 ? colorbar_magnitude_pixel_shader : colorbar_colorwheel_pixel_shader,
                        class_instances,
                        num_class_instances
                        );
                }
                
                d3d_device_context->IASetInputLayout(0);
                d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                {
                    uint const vertex_count = 4;
                    uint const start_vertex_location = 0;
                
                    d3d_device_context->Draw(
                        vertex_count,
                        start_vertex_location
                        );
                }                
                
            }
            
            
            d3d_device_context->IASetInputLayout(curve_vertex_input_layout);
            d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);                

            // NOTE: set the pixel shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->PSSetShader(
                    solid_pixel_shader,
                    class_instances,
                    num_class_instances
                    );
            }

            // NOTE: set the vertex shader
            {
                uint num_class_instances = 0;
                ID3D11ClassInstance** class_instances = 0;
                d3d_device_context->VSSetShader(
                    plot_vertex_shader,
                    class_instances,
                    num_class_instances
                    );
            }
            
            // NOTE: set the clip rectangle
            {
                float const plotviewport_min_y_screen =
                    (1.0f - plotviewport_max_y_viewport[plot_idx]) * viewport_y_unit_screen;
                float const plotviewport_max_y_screen =
                    (1.0f - plotviewport_min_y_viewport[plot_idx]) * viewport_y_unit_screen;
                
                
                D3D11_RECT rectangles[1];
				rectangles[0].left = LONG(plotviewport_min_x_screen);
				rectangles[0].top = LONG(plotviewport_min_y_screen);
				rectangles[0].right = LONG(plotviewport_max_x_screen);
				rectangles[0].bottom = LONG(plotviewport_max_y_screen);
                uint const num_rectangles = ARRAY_LENGTH(rectangles);
                d3d_device_context->RSSetScissorRects(
                    num_rectangles,
                    rectangles
                    );
                    
            }
            
            {
                uint const vertex_count = num_curve_slices;
                uint const start_vertex_location = 0;
                
                d3d_device_context->Draw(
                    vertex_count,
                    start_vertex_location
                    );
            }
            
        }
        
        
        
        {
            FLOAT* blend_factor = 0;
            UINT sample_mask = 0xffffffff;
            d3d_device_context->OMSetBlendState(blend_state, blend_factor, sample_mask);
        }        

        // TODO: unbind constant buffer? (so that it is not bound when updating it again next frame)
        
#if IIR4_WIDGET_PERFORMANCE_SPAM_LEVEL > 0        
        Platform::TimeCount work_end_counts = Platform::time_get_count();
#endif
        
        {
            UINT sync_interval = 0;
            UINT flags = 0;
            swap_chain->Present(sync_interval, flags);
        }

#if IIR4_WIDGET_PERFORMANCE_SPAM_LEVEL > 0        
        Platform::TimeCount present_time_counts = time_get_count();
        float work_duration = time_duration_seconds(work_start_counts, work_end_counts);
        float work_present_duration = time_duration_seconds(work_start_counts, present_time_counts);
#endif

        Platform::frame_end_sleep(&frame_context, target_frame_duration);

#if (IIR4_WIDGET_BUILDTYPE == IIR4_WIDGET_BUILDTYPE_INTERNAL) && (IIR4_WIDGET_PERFORMANCE_SPAM_LEVEL > 0)
        {
            float frame_duration_miss = current_frame_duration - target_frame_duration;
            float warning_threshold = 1.5E-3f;
            if( frame_duration_miss > warning_threshold)
            {
                log_string( "long frame \n" );
            }
            else if(frame_duration_miss < -warning_threshold)
            {
                log_string( "short frame \n" );
            }
        }
#endif

#if (IIR4_WIDGET_BUILDTYPE == IIR4_WIDGET_BUILDTYPE_INTERNAL) && (IIR4_WIDGET_PERFORMANCE_SPAM_LEVEL > 0)
        {

            Platform::TimeCount frame_end_counts = time_get_count();
            float work_total_duration = time_duration_seconds(work_start_counts, frame_end_counts);
            float frame_total_duration = time_duration_seconds(frame_context.start_counts, frame_end_counts);
            
            int num_decimal_places = 2;
            
            log_string("work: ");
            logarithm(work_duration*1.0E3f, num_decimal_places);
            log_string("ms");
            
            log_string(", ");

            log_string("work+present: ");
            logarithm(work_present_duration*1.0E3f, num_decimal_places);
            log_string("ms");
            
            log_string(", ");

            log_string("work+present+wakeup: ");
            logarithm(work_total_duration*1.0E3f, num_decimal_places);
            log_string("ms");
            
            log_string(", ");

            log_string("frame: ");
            logarithm(frame_total_duration*1.0E3f, num_decimal_places);
            log_string("ms");
            
            log_string("\n");
            
        }
#endif

        if(input_state.mouse_input_enabled)
        {
            time += target_frame_duration;
        }
        
    }

    // NOTE:
    // Once the main loop has been entered, this is assumed to be the only valid exit point.
    // That is to say, it is not "allowed" to return from the main loop, only to break out of it.
    // NOTE:
    // This is not strictly necessary since the app is about to exit.
    // However, decrementing the refcounts here avoids spamming the output window with warnings
    // as the application exits.
    swap_chain->Release();
    grid_numbers_constant_buffer->Release();
    font_texture->Release();
    font_texture_srv->Release();
    font_sampler_state->Release();
    ttf_font_texture->Release();
    ttf_font_texture_srv->Release();
    ttf_font_sampler_state->Release();
    d3d_device_context->Release();
    d3d_device->Release();
    circle_vertex_buffer->Release();
    circle_index_buffer->Release();        
    dynamic_constant_buffer->Release();
    layout_constant_buffer->Release();    
    circle_vertex_shader->Release();    
    solid_pixel_shader->Release();
    colorbar_magnitude_pixel_shader->Release();
    colorbar_colorwheel_pixel_shader->Release();
    dynamic_vertex_shader->Release();
    circle_vertex_input_layout->Release();    
    dynamic_vertex_input_layout->Release();
    render_target_view->Release();
    blend_state->Release();
    grid_numbers_vertex_shader->Release();
    font_pixel_shader->Release();
    plot_vertex_shader->Release();
    plot_constant_buffer->Release();
    curve_vertex_buffer->Release();
    curve_vertex_input_layout->Release();
    colorbar_vertex_shader->Release();
    
    return 0;
    
}
