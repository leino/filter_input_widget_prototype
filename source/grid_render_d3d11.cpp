
namespace Grid
{

    // NOTE: this is sent as a constant buffer to the shader, so use appropriate packing
#pragma pack(push, 4)
    struct TextShaderConstants
    {
        uint number_characters[20];
        float text_end_position;
        float text_middle_transverse_position;
        float text_alpha;
        int orientation;
        uint viewport_width_pixels;
        uint viewport_height_pixels;
        uint end_margin_pixels;
        uint character_spacing_pixels;
    };
#pragma pack(pop)

    // NOTE: this is sent as a constant buffer to the shader, so use appropriate packing
#pragma pack(push, 4)
    struct GridLinesShaderConstants
    {
        
        float offset;
        float spacing;
        float power_remainder;
        float lo;
        
        uint max_power;
        uint line_idx_offset;
        uint orientation;
        uint base;
        
        float hi;
        float __padding[3];
    };
#pragma pack(pop)

    // Hard-coded shader constants.
    // Changing these means you also need to make changes to the shaders (and vice versa!).
    namespace ShaderConstants
    {
        uint const GRID_CONSTANT_BUFFER_SLOT = 0;
        uint const TEXT_CONSTANT_BUFFER_SLOT = 1;
        uint const TEXT_SAMPLER_SLOT = 0;
        uint const FONT_TEXTURE_SLOT = 0;
    };

    bool
    try_update_grid_text_constants(
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11Buffer *const constant_buffer,
        TextShaderConstants const*const new_constants
        )
    {
        ID3D11Resource* resource = constant_buffer;
        uint subresource = 0;
        D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
        // OPTIMIZE: Could use D3D11_MAP_FLAG_DO_NOT_WAIT here, in order to not wait on GPU here.
        // Could also "doubble-buffer" the constant buffer.
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
            GRID_LOG_ERROR("failed to update the grid text constant buffer");
            assert(false);
            return false;
        }
            
        TextShaderConstants *const constants = (TextShaderConstants*)mapped_subresource.pData;
        *constants = *new_constants;
        d3d_device_context->Unmap(resource, subresource);

        return true;
    }

    bool
    try_update_grid_constants(
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11Buffer *const constant_buffer,
        GridLinesShaderConstants const*const new_constants
        )
    {
        ID3D11Resource* resource = constant_buffer;
        uint subresource = 0;
        D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
        // OPTIMIZE: Could use D3D11_MAP_FLAG_DO_NOT_WAIT here, in order to not wait on GPU here.
        // Could also "doubble-buffer" the constant buffer.
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
            GRID_LOG_ERROR("failed to update the layout constant buffer");
            assert(false);
            return false;
        }
            
        GridLinesShaderConstants *const constants = (GridLinesShaderConstants*)mapped_subresource.pData;
        *constants = *new_constants;
        d3d_device_context->Unmap(resource, subresource);

        return true;
    }
    
    void
    draw_grid_lines(
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11VertexShader *const vertex_shader,
        ID3D11PixelShader *const pixel_shader,
        ID3D11Buffer *const constant_buffer,
        GridLinesContext const*const gctx
        )
    {


        // NOTE: set the pixel shader
        {
            uint num_class_instances = 0;
            ID3D11ClassInstance** class_instances = 0;
            d3d_device_context->PSSetShader(
                pixel_shader,
                class_instances,
                num_class_instances
                );            
        }
            
            
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
            uint const num_buffers = 1;
            uint const start_slot = ShaderConstants::GRID_CONSTANT_BUFFER_SLOT;
            ID3D11Buffer* buffers[num_buffers] = {constant_buffer};
            d3d_device_context->VSSetConstantBuffers(
                start_slot,
                num_buffers,
                buffers
                );
        }

        {
            
            GridLinesShaderConstants gc;
            gc.base = gctx->base;
            gc.offset = gctx->offset;
            gc.spacing = gctx->spacing;
            gc.power_remainder = gctx->power_remainder;
            gc.lo = gctx->lo;
            gc.hi = gctx->hi;
            gc.max_power = gctx->max_power;
            gc.line_idx_offset = gctx->line_idx_offset;
            gc.orientation = gctx->orientation;
            
            bool const success = try_update_grid_constants(d3d_device_context, constant_buffer, &gc);
            assert(success);
        }
        d3d_device_context->IASetInputLayout(0);
        d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        
        {
            uint const instance_vertex_count = 2;
            uint const instance_count = gctx->num_visible_lines;
            uint const start_vertex_location = 0;
            uint const start_instance_location = 0;

            d3d_device_context->DrawInstanced(
                instance_vertex_count,
                instance_count,
                start_vertex_location,
                start_instance_location
                );
        }

    }

    void
    emit_grid_numbers_draw_calls(
        uint const character_spacing_screen,
        uint const viewport_width_pixels,
        uint const viewport_height_pixels,
        Orientation const orientation,
        int const max_num_significant_digits,
        GridLinesContext const*const grid_ctx,
        float const text_end_position_viewport,
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11Buffer *const grid_text_constant_buffer
        )
    {

        using namespace Numerics;
        using namespace GridNumberIterator;

        Context ctx = {};
        State st = {};

        for(
            initialize(
                grid_ctx,
                max_num_significant_digits,
                &ctx,
                &st
                );
            !done(&ctx, &st);
            step(&st)
            )
        {

            int const relative_grid_line_idx = st.line_idx - grid_ctx->min_visible_line_idx;
                
            // NOTE: calculate the highest power by which this grid line is divisible
            uint power = 0;
            {
                uint const grid_pattern_length =
                    Numerics::power(grid_ctx->base, grid_ctx->max_power);
                    
                uint n = (relative_grid_line_idx + grid_ctx->line_idx_offset);
                while( (n % grid_ctx->base) == 0)
                {
                    n /= grid_ctx->base;
                    power++;
                    if(power == grid_ctx->max_power)
                        break;
                }
            }

            TextShaderConstants constants = {};
            
            constants.viewport_width_pixels = viewport_width_pixels;
            constants.viewport_height_pixels = viewport_height_pixels;
            constants.orientation = orientation;
            constants.end_margin_pixels = 3;
            constants.character_spacing_pixels = character_spacing_screen;
            int number_string_length;
            number(&ctx, &st, constants.number_characters, &number_string_length);
                
            {
                int64 const grid_line_idx =
                    (grid_ctx->min_visible_line_idx + relative_grid_line_idx);

                int64 const abs_grid_line_idx =
                    Numerics::absolute_value(grid_line_idx);

                    
                constants.text_end_position = text_end_position_viewport;

                float const transverse_screen_unit_viewport =
                    orientation == Orientation::Horizontal ?
                    float(2)/float(viewport_height_pixels) : float(2)/float(viewport_width_pixels);
                    
                
                // NOTE: we clamp to integer-multiples of pixels because we cannot render font with sub-pixel
                // precision
                // TODO: get rid of this constraint, makes "crawling" effect when animating
                constants.text_middle_transverse_position =
                    floor(
                        (
                            grid_ctx->offset +
                            grid_ctx->spacing * float(relative_grid_line_idx)
                            )
                        /transverse_screen_unit_viewport
                        ) * transverse_screen_unit_viewport;


                float const alpha =
                    (float(power) + grid_ctx->power_remainder)/
                    float(grid_ctx->max_power+1.0f);
                    
                constants.text_alpha = alpha;
                    
                {
                    bool const success =
                        try_update_grid_text_constants(
                            d3d_device_context,
                            grid_text_constant_buffer,
                            &constants
                            );
                    assert(success);
                }
            
            }            
            
            {
                uint const instance_vertex_count = 4;
                uint const instance_count = number_string_length;
                uint const start_vertex_location = 0;
                uint const start_instance_location = 0;

                d3d_device_context->DrawInstanced(
                    instance_vertex_count,
                    instance_count,
                    start_vertex_location,
                    start_instance_location
                    );
            }
        }

    }    
    
    void
    draw_grid_numbers(
        uint const character_spacing_screen,
        uint const viewport_width_pixels,
        uint const viewport_height_pixels,
        float const horizontal_text_end_viewport,
        float const vertical_text_end_viewport,
        GridLinesContext const*const horizontal_grid_context,
        GridLinesContext const*const vertical_grid_context,
        ID3D11VertexShader *const font_vertex_shader,
        ID3D11PixelShader *const font_pixel_shader,
        ID3D11Buffer *const grid_text_constant_buffer,
        ID3D11SamplerState *const font_sampler_state,
        ID3D11ShaderResourceView *const font_texture_srv,
        ID3D11DeviceContext *const d3d_device_context
        )
    {
            
        // NOTE: set the pixel shader
        {
            uint num_class_instances = 0;
            ID3D11ClassInstance** class_instances = 0;
            d3d_device_context->PSSetShader(
                font_pixel_shader,
                class_instances,
                num_class_instances
                );
        }

        // NOTE: set the vertex shader
        {
            uint num_class_instances = 0;
            ID3D11ClassInstance** class_instances = 0;
            d3d_device_context->VSSetShader(
                font_vertex_shader,
                class_instances,
                num_class_instances
                );
        }            

        {
            uint const num_buffers = 1;
            uint const start_slot = ShaderConstants::TEXT_CONSTANT_BUFFER_SLOT;
            ID3D11Buffer* buffers[num_buffers] = {grid_text_constant_buffer};
            d3d_device_context->VSSetConstantBuffers(
                start_slot,
                num_buffers,
                buffers
                );
        }

        {
            uint const num_buffers = 1;
            ID3D11Buffer* buffers[num_buffers] = {grid_text_constant_buffer};
            uint const start_slot = ShaderConstants::TEXT_CONSTANT_BUFFER_SLOT;
            d3d_device_context->PSSetConstantBuffers(
                start_slot,
                num_buffers,
                buffers
                );            
            
        }            

        // NOTE: set font sampler
        {
            
            uint const start_slot = ShaderConstants::TEXT_SAMPLER_SLOT;
            ID3D11SamplerState *const samplers[] = {font_sampler_state};
            uint const num_samplers = ARRAY_LENGTH(samplers); 
            
            d3d_device_context->PSSetSamplers(
                start_slot,
                num_samplers,
                samplers
                );
            
        }
            
        {

            uint const start_slot = ShaderConstants::FONT_TEXTURE_SLOT;
            ID3D11ShaderResourceView *const shader_resource_views[] = {font_texture_srv};
            uint const num_views = ARRAY_LENGTH(shader_resource_views);
                
            d3d_device_context->PSSetShaderResources(
                start_slot,
                num_views,
                shader_resource_views
                );
                
        }
            
        d3d_device_context->IASetInputLayout(0);
        d3d_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        int const max_num_significant_digits = 12;
        int const max_num_exponent_digits = 2;

        int const max_length = GridNumberIterator::max_string_length(
            max_num_significant_digits, max_num_exponent_digits
            );

        assert(max_length <= ARRAY_LENGTH(TextShaderConstants::number_characters));

        // NOTE: horizontal numbers
        emit_grid_numbers_draw_calls(
            character_spacing_screen,
            viewport_width_pixels,
            viewport_height_pixels,
            Orientation::Horizontal, // NOTE: text goes horizontally
            max_num_significant_digits,
            horizontal_grid_context,
            horizontal_text_end_viewport,
            d3d_device_context,
            grid_text_constant_buffer
            );

        // NOTE: vertical numbers
        emit_grid_numbers_draw_calls(
            character_spacing_screen,
            viewport_width_pixels,
            viewport_height_pixels,
            Orientation::Vertical, // NOTE: text goes vertically
            max_num_significant_digits,
            vertical_grid_context,
            vertical_text_end_viewport,
            d3d_device_context,
            grid_text_constant_buffer
            );

    }

    void
    draw_grid(
        uint const character_spacing_screen,
        uint const viewport_width_pixels,
        uint const viewport_height_pixels,
        GridLinesContext const*const horizontal_context,
        GridLinesContext const*const vertical_context,
        float const horizontal_text_end_position_viewport,
        float const vertical_text_end_position_viewport,
        ID3D11Buffer *const grid_constant_buffer,
        ID3D11Buffer *const numbers_constant_buffer,
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11VertexShader *const grid_vertex_shader,
        ID3D11VertexShader *const numbers_vertex_shader,
        ID3D11PixelShader *const grid_pixel_shader,
        ID3D11PixelShader *const font_pixel_shader,
        ID3D11SamplerState *const font_sampler_state,
        ID3D11ShaderResourceView *const font_texture_srv
        )
    {

        draw_grid_lines(
            d3d_device_context,
            grid_vertex_shader,
            grid_pixel_shader,
            grid_constant_buffer,
            horizontal_context
            );

        draw_grid_lines(
            d3d_device_context,
            grid_vertex_shader,
            grid_pixel_shader,
            grid_constant_buffer,
            vertical_context
            );

        draw_grid_numbers(
            character_spacing_screen,
            viewport_width_pixels,
            viewport_height_pixels,
            horizontal_text_end_position_viewport,
            vertical_text_end_position_viewport,
            horizontal_context,
            vertical_context,
            numbers_vertex_shader,
            font_pixel_shader,
            numbers_constant_buffer,
            font_sampler_state,
            font_texture_srv,
            d3d_device_context
            );
        
    }
    
    void
    draw_grid(
        uint const character_spacing_screen,
        uint const base,
        float const smallest_visible_horizontal_level_spacing_viewport,
        float const smallest_visible_vertical_level_spacing_viewport,
        float const min_x_viewport,
        float const max_x_viewport,
        float const min_y_viewport,
        float const max_y_viewport,
        uint const viewport_width_pixels,
        uint const viewport_height_pixels,
        float const horizontal_text_end_position_viewport,
        float const vertical_text_end_position_viewport,
        Transform const*const horizontal_transform,
        Transform const*const vertical_transform,
        ID3D11DeviceContext *const d3d_device_context,
        ID3D11VertexShader *const grid_vertex_shader,
        ID3D11PixelShader *const grid_pixel_shader,
        ID3D11Buffer *const grid_constant_buffer,
        ID3D11VertexShader *const numbers_vertex_shader,
        ID3D11PixelShader *const font_pixel_shader,
        ID3D11SamplerState *const font_sampler_state,
        ID3D11ShaderResourceView *const font_texture_srv,
        ID3D11Buffer *const numbers_constant_buffer
        )
    {

        GridLinesContext horizontal_context;
        GridLinesContext vertical_context;
        
        grid(
            base,
            smallest_visible_horizontal_level_spacing_viewport,
            smallest_visible_vertical_level_spacing_viewport,
            min_x_viewport,
            max_x_viewport,
            min_y_viewport,
            max_y_viewport,
            horizontal_transform,
            vertical_transform,
            &horizontal_context,
            &vertical_context
            );
        
        draw_grid(
            character_spacing_screen,
            viewport_width_pixels,
            viewport_height_pixels,
            &horizontal_context,
            &vertical_context,
            horizontal_text_end_position_viewport,
            vertical_text_end_position_viewport,
            grid_constant_buffer,
            numbers_constant_buffer,
            d3d_device_context,
            grid_vertex_shader,
            numbers_vertex_shader,
            grid_pixel_shader,
            font_pixel_shader,
            font_sampler_state,
            font_texture_srv
            );        
        
    }
    
    
}
