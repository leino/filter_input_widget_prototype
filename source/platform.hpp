namespace Platform
{
    typedef LARGE_INTEGER TimeCount;
    
    struct ReadFileResult
    {
        void *contents;
        uint32 contents_size;
    };
    
    
    struct FrameContext
    {
        TimeCount start_counts;
    };

    struct InputContext
    {
        bool quit;
        int exit_code;
        bool mouse_input_toggled;
        Vec2::Vec2 mouse_delta_screen;
    };

    struct ButtonState
    {
        // Is the key pressed this frame?
        bool pressed;
        // Did the key change state during previous frame?
        bool changed_state;
    };

    struct InputState
    {
        bool mouse_input_enabled;

        ButtonState control;
        ButtonState quit;
        ButtonState mouse_left;
        ButtonState mouse_right;
        int mouse_wheel_delta;
    };

    inline bool
    got_pressed(ButtonState const*const st)
    {
        return st->changed_state && st->pressed;
    }

    inline bool
    got_released(ButtonState const*const st)
    {
        return st->changed_state && !st->pressed;
    }    
    
}

namespace Platform
{
    ReadFileResult read_file(char* file_name);
    void free_file_memory(void* address);
};

namespace Platform
{
    inline float time_duration_seconds(TimeCount start, TimeCount end);
    inline TimeCount time_get_count();
    void frame_start(FrameContext* ctx);
    void frame_end_sleep(FrameContext* ctx, float target_frame_duration);
    void get_input(InputContext* ctx, InputState* state);    
    bool init(uint viewport_width_screen, uint viewport_height_screen, bool mouse_input_initially_enabled);
};
