#define GRID_NUMBER_FONT_DECIMAL_POINT 10
#define GRID_NUMBER_FONT_PLUS 11
#define GRID_NUMBER_FONT_MINUS 12
#define GRID_NUMBER_FONT_EXPONENTIAL_MARKER 13

namespace Grid
{    

    uint const FONT_CHARACTER_X_DIMENSION_SCREEN = 5;
    uint const FONT_CHARACTER_Y_DIMENSION_SCREEN = 6;
    uint const FONT_NUM_CHARACTERS = 14;
    uint const FONT_TEXTURE_X_DIMENSION_SCREEN = FONT_NUM_CHARACTERS*FONT_CHARACTER_X_DIMENSION_SCREEN;
    uint const FONT_TEXTURE_Y_DIMENSION_SCREEN = FONT_CHARACTER_Y_DIMENSION_SCREEN;

    // NOTE: width of message in screen space (in pixels)
    uint const
    message_width_screen(uint const character_spacing_screen, uint const num_characters)
    {
        uint const num_spaces = num_characters - 1;
        return num_spaces*character_spacing_screen + num_characters*FONT_CHARACTER_X_DIMENSION_SCREEN;
    }
    
    enum Orientation
    {
        Horizontal,
        Vertical,
        NumOrientations
    };

    // NOTE: make sure that the enums will match with what the shaders expect
    static_assert(
        int(Orientation::Horizontal) == GRID_ORIENTATION_HORIZONTAL,
        "enum needs to match defined constant"
        );
    static_assert(
        int(Orientation::Vertical) == GRID_ORIENTATION_VERTICAL,
        "enum needs to match defined constant"
        );

    struct GridLinesContext
    {
        uint num_visible_lines;
        int lowest_visible_level_idx;
        int min_visible_line_idx;
        float offset;
        float spacing;
        float power_remainder;
        float lo;
        float hi;
        uint max_power;
        uint line_idx_offset;
        uint orientation;
        uint base;
    };

    // NOTE: We specify a viewport <-> data transform by specifying bounds of the viewport in data space.
    struct Transform
    {
        float viewport_min_data;
        float viewport_max_data;
    };
    
    namespace GridNumberIterator
    {
    
        struct Context
        {
            uint base;
            int min_line_idx;
            int max_line_idx;
            int decimal_point_idx;
            int num_digits;
            int power_offset;
            int exponent;
            int num_significant_digits;
        };

        struct State
        {
            int line_idx;
        };
        
    }    
    
    void
    grid_lines(
        uint const base,
        float const smallest_visible_level_spacing_viewport,
        Orientation const orientation,
        float const viewport_min_viewport,
        float const viewport_max_viewport,
        float const min_transverse_viewport,
        float const max_transverse_viewport,
        Transform const*const transform,
        GridLinesContext *const ctx
        )
    {
        
        float const viewport_min_data = transform->viewport_min_data;
        float const viewport_max_data = transform->viewport_max_data;
        float const viewport_center_data = (viewport_min_data + viewport_max_data)/2.0f;
        float const viewport_length_viewport = viewport_max_viewport - viewport_min_viewport;
        float const viewport_length_data = viewport_max_data - viewport_min_data;
        float const data_unit_viewport =
            viewport_length_viewport / viewport_length_data;

        // NOTE: smallest/largest visible level spacing in pixels
        assert(viewport_min_viewport < viewport_max_viewport);
        float const largest_visible_level_spacing_viewport = viewport_length_viewport;
    
        /*

          Grid level spacing in data coordinates is simply

          grid_spacing_data(level_idx) = base^level_idx

          In viewport coordinates, we get:

          grid_spacing_viewport(level_idx) = 
          base^(level_idx)*data_unit_viewport = 
          base^(level_idx + beta)
          where we define for brevity: beta = log(base, data_unit_viewport)

        */

        float const beta =
            Numerics::logarithm(float(base), data_unit_viewport);

        /*

          The lowest visible level_idx is the smallest integer which satisfies
          grid_spacing_viewport(level_idx) > smallest_visible_spacing_viewport
          In other words
          base^(level_idx + beta) > smallest_visible_spacing_viewport
          level_idx + beta > log(base, smallest_visible_spacing_viewport)
          level_idx > log(base, smallest_visible_spacing_viewport) - beta

          The highest visible level_idx is the largest integer which satisfies
          grid_spacing_viewport(level_idx) < largest_visible_spacing_viewport
          base^(level_idx + beta) < largest_visible_spacing_viewport
          level_idx + beta < log(base, largest_visible_spacing_viewport)
          level_idx < log(base, largest_visible_spacing_viewport) - beta

        */

        float const lowest_visible_power =
            Numerics::logarithm(float(base), smallest_visible_level_spacing_viewport) - beta;
        float const highest_visible_power =
            Numerics::logarithm(float(base), largest_visible_level_spacing_viewport) - beta;
        float const lowest_visible_level_idx =
            Numerics::ceiling(lowest_visible_power);

        // NOTE: the lines have to be spaced by the following distance
        float const lowest_visible_level_spacing_viewport =
            Numerics::power(float(base), lowest_visible_level_idx + beta);

        /*

          What is the viewport position of the leftmost visible line?
          We know the lowest visible level index, so leftmost_visible_line_idx for that level must be the 
          smallest integer which satisfies

          grid_spacing_data(lowest_visible_level_idx) * leftmost_visible_line_idx >= min_data
          base^(lowest_visible_level_idx) * leftmost_visible_line_idx >= min_data

          In other words

          leftmost_visible_line_idx = ceil( min_data / pow(base, lowest_visible_level_idx) )

          The offset of this line is 

          offset_data = leftmost_visible_line_idx * base^(lowest_visible_level_idx) - min_data
          offset_viewport = data_unit_viewport * offset_data

        */

        float const leftmost_visible_line_idx =
            Numerics::ceiling( viewport_min_data / Numerics::power(float(base), lowest_visible_level_idx) );
        float const rightmost_visible_line_idx =
            Numerics::floor( viewport_max_data / Numerics::power(float(base), lowest_visible_level_idx) );

        uint const num_visible_lines = (uint)(rightmost_visible_line_idx - leftmost_visible_line_idx) + 1;
    
        float const offset_data =
            leftmost_visible_line_idx * Numerics::power(float(base), lowest_visible_level_idx) - viewport_min_data;
        float const offset_viewport =
            viewport_min_viewport + data_unit_viewport * offset_data;
    
        // OPTIMIZE: this is very constant and does not need to be updated all the time
        uint const max_power =
            (uint)Numerics::ceiling(
                Numerics::logarithm(float(base), viewport_length_viewport/smallest_visible_level_spacing_viewport)
                );

        /*

          We need to calculate where in the repeating grid pattern (of length base^max_power) our leftmost
          grid line falls

        */

        uint const pattern_length = Numerics::power(base, max_power);
        uint const line_idx_offset = (uint)Numerics::remainder((int)pattern_length, (int)leftmost_visible_line_idx);
    
        float const power_remainder = lowest_visible_level_idx - lowest_visible_power;
        assert(power_remainder >= 0.0f);
        assert(power_remainder < 1.0f);

        ctx->line_idx_offset = line_idx_offset;
        ctx->offset = offset_viewport;
        ctx->power_remainder = power_remainder;
        ctx->spacing = lowest_visible_level_spacing_viewport;
        ctx->max_power = max_power;
        ctx->lo = min_transverse_viewport;
        ctx->hi = max_transverse_viewport;
        ctx->orientation = int(orientation);
        ctx->num_visible_lines = num_visible_lines;
        ctx->lowest_visible_level_idx = int(lowest_visible_level_idx);
        ctx->min_visible_line_idx = int(leftmost_visible_line_idx);
		ctx->base = base;
    }

    // Convenience function: both horizontal and vertical grid lines in a singgle call
    void
    grid(
        uint const base,
        float const smallest_visible_horizontal_level_spacing_viewport,
        float const smallest_visible_vertical_level_spacing_viewport,
        float const min_x_viewport,
        float const max_x_viewport,
        float const min_y_viewport,
        float const max_y_viewport,
        Transform const*const horizontal_transform,
        Transform const*const vertical_transform,
        GridLinesContext *const horizontal_grid_context,
        GridLinesContext *const vertical_grid_context
        )
    {

        // horizontal
        grid_lines(
            base,
            smallest_visible_horizontal_level_spacing_viewport,
            Grid::Orientation::Horizontal,
            min_y_viewport,
            max_y_viewport,
            min_x_viewport,
            max_x_viewport,
            horizontal_transform,
            horizontal_grid_context
            );

        // vertical
        grid_lines(
            base,
            smallest_visible_vertical_level_spacing_viewport,
            Grid::Orientation::Vertical,
            min_x_viewport,
            max_x_viewport,
            min_y_viewport,
            max_y_viewport,
            vertical_transform,
            vertical_grid_context
            );

    }    
    
    namespace GridNumberIterator
    {
    
        // NOTE: This is the longest length that the string can get with the given parameters
        constexpr int
        max_string_length(int const max_num_significant_digits, int const max_num_exponent_digits)
        {
            return
                4 +
                max_num_significant_digits + 
                max_num_exponent_digits
                ;
        }
    
        void
        initialize(
            GridLinesContext const*const grid_ctx,
            int const max_num_significant_digits,
            Context *const ctx,
            State *const st
            )
        {

            int const min_line_idx = grid_ctx->min_visible_line_idx;
            int const max_line_idx = min_line_idx + grid_ctx->num_visible_lines - 1;
            int const level_idx = grid_ctx->lowest_visible_level_idx;
        
            using namespace Numerics;
        
            ctx->num_significant_digits =
                1 + Numerics::maximum(
                    min_line_idx ==
                    0 ? 1 :
                    (int)floor(logarithm(float(grid_ctx->base), float(absolute_value(min_line_idx)))),
                    max_line_idx ==
                    0 ? 1 :
                    (int)floor(logarithm(float(grid_ctx->base), float(absolute_value(max_line_idx))))
                    );

            assert(ctx->num_significant_digits > 0);
            assert(ctx->num_significant_digits <= max_num_significant_digits);


            ctx->power_offset =
                clamp(
                    1 - max_num_significant_digits,
                    max_num_significant_digits - ctx->num_significant_digits,
                    level_idx
                    );
            ctx->exponent = level_idx - ctx->power_offset;
        
            ctx->num_digits =
                ctx->num_significant_digits +
                maximum(0, ctx->power_offset, 1 - ctx->power_offset - ctx->num_significant_digits);
        
            ctx->decimal_point_idx = ctx->num_digits + minimum(0, ctx->power_offset);

            assert(ctx->decimal_point_idx >= 1);
            assert(ctx->decimal_point_idx <= ctx->num_digits);    
            assert(min_line_idx <= max_line_idx);

            ctx->min_line_idx = min_line_idx;
            ctx->max_line_idx = max_line_idx;
            ctx->base = grid_ctx->base;
            st->line_idx = max_line_idx;
        
        }

        void
        step(State *const st)
        {
            st->line_idx--;
        }
    
        bool
        done(Context const*const ctx, State const*const st)
        {
            return st->line_idx < ctx->min_line_idx;
        }

        void
        number(Context const*const ctx, State const*const st, uint *const string, int *const length)
        {
            using namespace Numerics;
            int character_idx = 0;

            // NOTE: special case for zero
            if(st->line_idx == 0)
            {
                string[0] = 0;
                *length = 1;
            }
            else
            {
            
                int const abs_line_idx = absolute_value(st->line_idx);
                bool const print_sign = ctx->min_line_idx < 0 || ctx->max_line_idx < 0;
                bool const skip_leading_zeros = !print_sign && abs_line_idx != 0 && ctx->decimal_point_idx > 1;
                int const num_fractional_digits = ctx->num_digits - ctx->decimal_point_idx;
                int num_skipped_leading_digits = 0;
                if(skip_leading_zeros)
                {
                    int const num_leading_zeros =
                        (int)ceiling(
                            float(ctx->num_significant_digits) - logarithm(float(ctx->base), float(abs_line_idx))
                            )
                        -
                        1;

                    num_skipped_leading_digits = num_leading_zeros;
                }
                bool const print_exponent = ctx->exponent != 0 && abs_line_idx != 0;


                int digit_idx = 0;

                if(print_exponent)
                {
            
                    uint const abs_exponent = absolute_value(ctx->exponent);
                    int const num_exponent_digits =
                        abs_exponent <= 1.0f ?
                        1 :
                        1 + (int)floor(logarithm(float(ctx->base), float(abs_exponent)));
                    assert(num_exponent_digits >= 1);
                    for(int exponent_digit_idx=0; exponent_digit_idx < num_exponent_digits; exponent_digit_idx++)
                    {
                        uint const p = exponent_digit_idx;
                        uint const r =
                            remainder(
                                ctx->base,
                                abs_exponent / power(ctx->base, p)
                                );
                        string[character_idx++] = r;
                    }

                    // NOTE: exponent sign
                    if(ctx->exponent < 0)
                    {
                        string[character_idx++] = GRID_NUMBER_FONT_MINUS;
                    }
                    else if(ctx->exponent > 0)
                    {
                        string[character_idx++] = GRID_NUMBER_FONT_PLUS;
                    }
            
            
                    // NOTE: exponential marker
                    string[character_idx++] = GRID_NUMBER_FONT_EXPONENTIAL_MARKER;
            
                }

                // NOTE: digits after decimal point

                while(digit_idx < num_fractional_digits)
                {
                    uint const p = digit_idx;
                    assert(p >= 0);
            
                    uint const r =
                        remainder(
                            ctx->base,
                            (power(ctx->base, (uint)maximum(0, ctx->power_offset)) * abs_line_idx) /
                            power(ctx->base, p)
                            );

                    string[character_idx++] = r;
                    digit_idx++;
                }
        
                // NOTE: decimal point only if there are more digits
                if(num_fractional_digits > 0)
                {
                    string[character_idx++] = GRID_NUMBER_FONT_DECIMAL_POINT;
                }
        
                // NOTE: digits before decimal point
                while(digit_idx < ctx->num_digits - num_skipped_leading_digits)
                {
                    uint const p = digit_idx;
                    assert(p >= 0);            
                    uint const r =
                        remainder(
                            ctx->base,
                            (power(ctx->base, (uint)maximum(0, ctx->power_offset)) * abs_line_idx) /
                            power(ctx->base, p)
                            );
            
                    string[character_idx++] = r;
                    digit_idx++;
                }
        
                // NOTE: sign
                if(print_sign)
                {
                    if(st->line_idx < 0)
                    {
                        string[character_idx++] = GRID_NUMBER_FONT_MINUS;
                    }
                    else if(st->line_idx > 0)
                    {
                        string[character_idx++] = GRID_NUMBER_FONT_PLUS;
                    }
                }        
        
        
                *length = character_idx;
            }
        
        }
    
    }
    
    inline void
    font_texture(uint32 pixels[FONT_TEXTURE_X_DIMENSION_SCREEN*FONT_TEXTURE_Y_DIMENSION_SCREEN])
    {
        
        uint32 const w = 0x000000ff;
        uint32 const b = 0x00000000;
        uint32 const p[FONT_TEXTURE_X_DIMENSION_SCREEN*FONT_TEXTURE_Y_DIMENSION_SCREEN] = 
            {
                //      0          1          2          3          4          5          6          7          8          9          .          +          -          e
                b,w,w,w,b, b,b,w,b,b, b,w,w,w,b, b,w,w,w,b, w,b,b,b,w, w,w,w,w,w, b,w,w,w,w, w,w,w,w,w, b,w,w,w,b, b,w,w,w,b, b,b,b,b,b, b,b,b,b,b, b,b,b,b,b, b,b,b,b,b,
                w,b,b,b,w, b,w,w,b,b, w,b,b,b,w, w,b,b,b,w, w,b,b,b,w, w,b,b,b,b, w,b,b,b,b, b,b,b,b,w, w,b,b,b,w, w,b,b,b,w, b,b,b,b,b, b,b,w,b,b, b,b,b,b,b, b,w,w,w,b,
                w,b,b,b,w, w,b,w,b,b, b,b,b,b,w, b,b,w,w,b, w,w,w,w,w, w,w,w,w,b, w,w,w,w,b, b,b,b,b,w, b,w,w,w,b, b,w,w,w,w, b,b,b,b,b, b,b,w,b,b, b,b,b,b,b, w,b,b,b,w,
                w,b,b,b,w, b,b,w,b,b, b,w,w,w,b, b,b,b,b,w, b,b,b,b,w, b,b,b,b,w, w,b,b,b,w, b,b,b,w,b, w,b,b,b,w, b,b,b,b,w, b,b,w,b,b, w,w,w,w,w, w,w,w,w,w, w,w,w,w,b,
                w,b,b,b,w, b,b,w,b,b, w,b,b,b,b, w,b,b,b,w, b,b,b,b,w, w,b,b,b,w, w,b,b,b,w, b,b,w,b,b, w,b,b,b,w, w,b,b,b,w, b,w,w,w,b, b,b,w,b,b, b,b,b,b,b, w,b,b,b,b,
                b,w,w,w,b, w,w,w,w,w, w,w,w,w,w, b,w,w,w,b, b,b,b,b,w, b,w,w,w,b, b,w,w,w,b, b,w,b,b,b, b,w,w,w,b, b,w,w,w,b, b,b,w,b,b, b,b,w,b,b, b,b,b,b,b, b,w,w,w,w
                
            };
        
        memcpy(pixels, p, sizeof(p));
        
    }
    
}
