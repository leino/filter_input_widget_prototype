static bool g_sleep_is_granular = false;
static HINSTANCE g_instance = 0;

#define RAWINPUT_BUFFER_SIZE 1024
uchar rawinput_buffer[RAWINPUT_BUFFER_SIZE];


static uint64 global_perfcounter_frequency;


void clear_transient_input_state(Platform::InputState* input)
{
    input->control.changed_state = false;
    input->quit.changed_state = false;
    
    input->mouse_left.changed_state = false;
    input->mouse_right.changed_state = false;
    input->mouse_wheel_delta = 0;
}


namespace Platform
{
    inline void
    log_string(char const*const msg)
    {
        // TODO: implement better logging
#if 0
        OutputDebugStringA(msg);
#else
        printf(msg);
        fflush(stdout);
#endif
    }

    inline void log_line(void)
    {
        log_string("\n");
    }
    
    inline void
    log_line_string(char const*const msg)
    {
        log_string(msg);
        log_string("\n");
    }

    void
    log_uint32(int const n)
    {
        // NOTE: 
        // We need a buffer size of ceil(log10(2^32))=10,
        // plus one byte for zero termination
        size_t const size = 10+1;
        char buffer[size];
        int needed_buffer_size = _snprintf(buffer, size, "%d", n);
        if(needed_buffer_size > size)
        {
            log_string("warning: log_uint32 buffer size too small");
            return;
        }
        else if(needed_buffer_size == size)
        {
            log_string("warning: log_uint32 buffer size too small to append a null terminator");
            return;
        }
        assert( buffer[needed_buffer_size] == '\0' );
        log_string(buffer);
    }

    void
    log_int(int const n)
    {
        // NOTE:
        // We need (at most) a buffer size of ceil(log10(2^32))=10,
        // plus one byte for the sign bit, and one for zero termination
        size_t const size = 10+1+1;
        char buffer[size];
        int needed_buffer_size = _snprintf(buffer, size, "%d", n);
        if(needed_buffer_size > size)
        {
            log_string("warning: log_int buffer size too small");
            return;
        }
        else if(needed_buffer_size == size)
        {
            log_string("warning: log_int buffer size too small to append a null terminator");
            return;
        }
        assert( buffer[needed_buffer_size] == '\0' );
        log_string(buffer);
    }

    void
    log_float32(float const x)
    {
    
        // TODO: find out how long a floating point number can be when printed
        // out as a string
        size_t const size = 100;
    
        char buffer[size];
        int needed_buffer_size = _snprintf(buffer, size, "%f", x);
        if(needed_buffer_size > size)
        {
            log_string("warning: log_float32 buffer size too small");
            return;
        }
        else if(needed_buffer_size == size)
        {
            log_string("warning: log_float32 buffer size too small to append a null terminator");
            return;
        }
        assert( buffer[needed_buffer_size] == '\0' );
        log_string(buffer);
    }

    inline void
    log_float(float x)
    {
        log_float32(x);
    }
    
};


void process_pending_messages(
    Platform::InputState* input,
    bool* quit,
    int* exit_code,
    int32* mouse_delta_x_dots,
    int32* mouse_delta_y_dots,
    bool* mouse_input_toggled
    )
{

    MSG message = {};
    // NOTE: this means "don't filter messages"
    UINT message_filter_min = 0;
    UINT message_filter_max = 0;
    // NOTE: retrieve all messages!
    HWND handle = 0;
    // NOTE: remove after peeking
    UINT remove_message = PM_REMOVE;
    
    while( true )
    {

        BOOL message_available =
            PeekMessage(
                &message,
                handle,
                message_filter_min,
                message_filter_max,
                remove_message
                );

        if(!message_available)
            break;
        
        switch(message.message)
        {

        case WM_QUIT:
        {
            *exit_code = (int)message.wParam;
            *quit = true;
        } break;
            
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {

            // TODO: raw keyboard input?
            
            uint32 vk_code = (uint32)message.wParam;
            bool was_down = (message.lParam & (1 << 30)) != 0;
            bool is_down = (message.lParam & (1 << 31)) == 0;
        
            if(was_down != is_down)
            {

                if(vk_code == VK_F1 && is_down)
                {
                    *mouse_input_toggled = true;
                }

                {
                    Platform::ButtonState* button = 0;
                
                    if(vk_code == VK_ESCAPE)
                        button = &input->quit;
                    else if(vk_code == VK_CONTROL)
                        button = &input->control;
                    
                    
                    if(button != 0)
                    {
                        button->changed_state = true;
                        button->pressed = is_down;
                    }
                }
                
            }

        } break;

        case WM_INPUT:
        {
            { // accumulate mouse delta, x and y, dots
                
                HRAWINPUT raw_input = (HRAWINPUT)message.lParam;
                UINT command = RID_INPUT;
                UINT header_size = sizeof(RAWINPUTHEADER);

                // NOTE: find out how much memory is required to get the input data
                UINT num_bytes_required = 0;
                {
                    LPVOID data = 0; // NOTE: We pass in NULL in order to get the required size returned
                    UINT result = GetRawInputData(raw_input, command, data, &num_bytes_required, header_size);
                    bool const error = result == (UINT)-1;
                    if( error )
                    {
                        Platform::log_line_string("error while getting raw input data (required size)");
                    }
                    else if(num_bytes_required > RAWINPUT_BUFFER_SIZE)
                    {
                        Platform::log_line_string("error: number of bytes required to read raw input was too high (");
                        Platform::log_uint32(num_bytes_required);
                        Platform::log_line_string(")");
                        // TODO: do dynamic (re)allocation here, and log a warning
                    }
                }

                assert(num_bytes_required <= RAWINPUT_BUFFER_SIZE);

                { // read the data
                    LPVOID data = (void*)rawinput_buffer;
                    UINT data_size = num_bytes_required;
                    UINT num_bytes_read = GetRawInputData(raw_input, command, data, &data_size, header_size);
                    bool const error = num_bytes_read == (UINT)-1;
                    if(error)
                    {
                        Platform::log_line_string("error: reading raw input (data)");
                    }
                    else
                    {
                        bool const inconsistent_output = num_bytes_read != data_size;
                        if(inconsistent_output)
                        {
                            Platform::log_line_string("warning: inconsistent output when reading raw input");
                        }
                    
                        RAWINPUT* input_data = (RAWINPUT*)data;
                        if(input_data->header.dwType == RIM_TYPEMOUSE)
                        {
                            RAWMOUSE* mouse = &input_data->data.mouse;


                            { // warn about unhandled input types
                                USHORT flags = mouse->usFlags;
                            
                                if( flags & MOUSE_ATTRIBUTES_CHANGED )
                                {
                                    Platform::log_line_string(
                                        "warning: unhandled mouse input: MOUSE_ATTRIBUTES_CHANGED"
                                        );
                                    assert(false);
                                }
                                
                                if( flags & MOUSE_MOVE_ABSOLUTE )
                                {
                                    Platform::log_line_string("warning: unhandled mouse input: MOUSE_MOVE_ABSOLUTE");
                                    assert(false);
                                }
                            
                                if( flags & MOUSE_VIRTUAL_DESKTOP )
                                {
                                    Platform::log_line_string(
                                        "warning: unhandled mouse input: MOUSE_VIRTUAL_DESKTOP"
                                        );
                                    assert(false);
                                }                            
                            }
                                                        
                            
                            // NOTE: lLastX and lLasyY are (relative or absolute) "dots" that the
                            // mouse has moved. "dots" as in DPI or "dots per inch".
                            // What this means is that if the mouse is moved sufficiently slowly
                            // by an inch in the positive x direction, then lLastX will move trough DPI dots,
                            // which is a mouse-specific number that is apparently not queryable.
                            // (It seems that most games, and windows itself, get's around this fact by
                            //  simply haiving mouse sensitivity sliders and leaving calibration to the user.
                            *mouse_delta_x_dots += mouse->lLastX;
                            *mouse_delta_y_dots += mouse->lLastY;

                            USHORT button_flags = mouse->usButtonFlags;
                            
                            bool left_button_down = (button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0;
                            bool left_button_up = (button_flags & RI_MOUSE_LEFT_BUTTON_UP) != 0;
                            if( left_button_up )
                            {
                                assert(!left_button_down);
                                input->mouse_left.changed_state = input->mouse_left.pressed;
                                input->mouse_left.pressed = false;
                            }
                            if( left_button_down )
                            {
                                assert(!left_button_up);
                                input->mouse_left.changed_state = !input->mouse_left.pressed;
                                input->mouse_left.pressed = true;
                            }

                            bool right_button_down = (button_flags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0;
                            bool right_button_up = (button_flags & RI_MOUSE_RIGHT_BUTTON_UP) != 0;
                            if( right_button_up )
                            {
                                assert(!right_button_down);
                                input->mouse_right.changed_state = input->mouse_right.pressed;
                                input->mouse_right.pressed = false;
                            }
                            if( right_button_down )
                            {
                                assert(!right_button_up);
                                input->mouse_right.changed_state = !input->mouse_right.pressed;
                                input->mouse_right.pressed = true;
                            }
                            
                            bool wheel_scrolled = (button_flags & RI_MOUSE_WHEEL) != 0;
                            if(wheel_scrolled)
                            {
                                SHORT wheel_delta = mouse->usButtonData;
                                input->mouse_wheel_delta = (int)wheel_delta;
                            }
                            
                        }
                    }
                }
            }

        } break;
        
        
        default:
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        } break;
        
        }
        
    }
    
}

void set_mouse_trapping(HWND window_handle, bool trap)
{

    if(!trap)
    {
        RECT* trap_rectangle = 0;
        BOOL success = ClipCursor( trap_rectangle );
        if(!success)
        {
            Platform::log_line_string("failed to untrap the mouse");
        }
    }
    else
    {
        RECT client_rectangle;
        // Get client rect in "window space"

        if( !GetClientRect( window_handle, &client_rectangle ) )
        {
            Platform::log_line_string("failed to get client rectangle for mouse mouse trapping");
        }
        else
        {
            POINT top_left = { client_rectangle.left, client_rectangle.top };
            {
                BOOL success = ClientToScreen(window_handle, &top_left);
                if(!success)
                {
                    Platform::log_line_string(
                        "failed to convert top left corner of client rectangle into screen coordinates "
                        "while trapping mouse"
                        );
                }
            }

            POINT bottom_right = { client_rectangle.right, client_rectangle.bottom };                
            {
                BOOL success = ClientToScreen(window_handle, &bottom_right);                    
                if(!success)
                {
                    Platform::log_line_string(
                        "failed to convert bottom right corner of client rectangle into screen"
                        "coordinates while trapping mouse"
                        );
                }
            }

            {
                BOOL success =
                    SetRect(&client_rectangle, top_left.x, top_left.y, bottom_right.x, bottom_right.y);
                        
                if(!success)
                {
                    Platform::log_line_string(
                        "failed to set the desired client rectangle while trapping mouse "
                        "while trapping mouse"
                        );
                }
            }

            {
                BOOL success = ClipCursor( &client_rectangle );
                if(!success)
                {
                    Platform::log_line_string("failed to trap the mouse");
                }
            }
        }
                        
    }
                    
}

namespace Platform
{

    void free_file_memory(void* address)
    {
        SIZE_T size = 0;
        DWORD free_type = MEM_RELEASE;
        BOOL success = VirtualFree(address, size, free_type);
        if(!success)
            Platform::log_line_string("freeing file memory failed");
        assert(success);
    }

    // NOTE: caller gets to free the file using free_file_memory
    ReadFileResult read_file(char* file_name)
    {
        Platform::ReadFileResult result = {};

        //// open the file handle
        HANDLE file_handle = 0;    
        {
            DWORD desired_access = GENERIC_READ;
            DWORD share_mode = FILE_SHARE_READ; // NOTE: others may read the file
            LPSECURITY_ATTRIBUTES security_attributes = 0;
            DWORD creation_disposition = OPEN_EXISTING;
            DWORD flags_and_attributes = 0;
            HANDLE template_file_handle = 0;
        
            file_handle = CreateFile(file_name,
                                     desired_access,
                                     share_mode,
                                     security_attributes,
                                     creation_disposition,
                                     flags_and_attributes,
                                     template_file_handle);

            if(file_handle == INVALID_HANDLE_VALUE)
                return result;
        }
        assert( file_handle != 0 );
    
        //// get the file size
        // NOTE: to simplify things, we require that the file be less than 4 Gb in size.
        uint32 file_size = 0;
        {
            LARGE_INTEGER file_size_64;
            BOOL success = GetFileSizeEx(file_handle, &file_size_64);
            if( success == FALSE )
            {
                Platform::log_line_string("failed to get file size");
                return result;
            }
            else if(file_size_64.QuadPart >= UINT32_MAX)
            {
                Platform::log_line_string("file size is too large");
                return result;
            }
            else
            {
                file_size = (uint32)file_size_64.QuadPart;
            }
        }

        //// allocate memory
        {
            LPVOID address = 0; // zero means windows decides
            SIZE_T size = file_size;
            DWORD allocation_type = MEM_RESERVE | MEM_COMMIT;
            DWORD protection = PAGE_READWRITE; 

            result.contents = VirtualAlloc(address, size, allocation_type, protection);

            if(result.contents == 0)
            {
                Platform::log_line_string("not enough memory\n");
                return result;
            }

            result.contents_size = file_size;
        }
        assert(result.contents != 0);

        { // read file into buffer
            DWORD num_bytes_read = 0;
            DWORD num_bytes_to_read = result.contents_size;
            LPOVERLAPPED overlapped = 0;

            BOOL success = ReadFile(file_handle,
                                    result.contents,
                                    num_bytes_to_read,
                                    &num_bytes_read,
                                    overlapped);
    
            if( !success || num_bytes_read != num_bytes_to_read )
            {
                Platform::log_line_string("reading file failed\n");
                free_file_memory( result.contents );
                result.contents = 0;
                result.contents_size = 0;
                return result;
            }
        }

        { // close the file
            BOOL success = CloseHandle(file_handle);
            if( !success )
            {
                Platform::log_line_string("closing file failed\n");
                return result;
            }
        }

        return result;
    }
    
    
};

namespace Platform
{    
    
    inline float
    time_duration_seconds(TimeCount start, TimeCount end)
    {
        float result =
            (float)(end.QuadPart - start.QuadPart) /
            (float)global_perfcounter_frequency;
        return result;
    }    

    inline TimeCount
    time_get_count()
    {
        TimeCount result;
        QueryPerformanceCounter(&result);
        return result;
    }    

    void frame_start(FrameContext* ctx)
    {
        ctx->start_counts = time_get_count();
    }    
    
    void frame_end_sleep(FrameContext* ctx, float target_frame_duration)
    {

        float current_frame_duration =
            time_duration_seconds(ctx->start_counts, time_get_count());
            
        // STUDY: when sleep is "granular" we sometimes still oversleep by about 4 ms or so!
        // Why is that?
        // NOTE: Spin-locking gives a reliable framerate. (put sleep_is_granular == false)
        // NOTE: A workaround: we sleep a millisecond at a time, so that we can wake up
        // with finer granularity
        float spinlock_duration = 0.5E-3f;
        if( g_sleep_is_granular )
        {
            bool busy = false;
            while( target_frame_duration > current_frame_duration )
            {
                DWORD duration_milliseconds =
                    (DWORD)floorf(1.0E3f * (target_frame_duration - current_frame_duration));

                // break out so that we spin lock for the last millisecond
                // (or fraction thereof)
                if( duration_milliseconds <= 1 )
                    break;

                if(busy){
                    // spin lock for half a millisecond
                    TimeCount start = Platform::time_get_count();
                    while(time_duration_seconds(start, time_get_count()) < spinlock_duration);
                }else{
                    Sleep(1);
                }

                busy = !busy;
                    
                current_frame_duration =
                    Platform::time_duration_seconds(ctx->start_counts, time_get_count());
            }
        }

        // spin-lock if we woke up too early (or sleep is not granular)
        while(current_frame_duration < target_frame_duration)
        {
            current_frame_duration =
                Platform::time_duration_seconds(ctx->start_counts, time_get_count());
        }
    }

    // NOTE:
    // This is the position of the system cursor,
    // with acceleration applied and in coordinates snapped to integers!
    bool try_get_system_cursor_global_position(int *const x, int *const y)
    {

        POINT point;
        BOOL const success = GetCursorPos(
            &point
            );
        
        if(!success)
        {
            // TODO: call GetLastError
            return false;
        }
        else
        {
            
            *x = point.x;
            *y = point.y;
            
            return true;
        }
        
    }


    // NOTE:
    // This is the position of the system cursor,
    // with acceleration applied and in coordinates snapped to integers!
    bool try_get_system_cursor_local_position(int *const x, int *const y)
    {

        POINT point;
        BOOL const success = GetCursorPos(
            &point
            );
        
        if(!success)
        {
            // TODO: call GetLastError
            return false;
        }
        else
        {

            BOOL const conversion_success = ScreenToClient(
                g_window_handle,
                &point
                );

            if(!conversion_success)
            {
                return false;
            }
            
            *x = point.x;
            *y = point.y;
            
            return true;
        }
        
    }    
    
    void get_input(InputContext* ctx, InputState* state)
    {

        clear_transient_input_state(state);
        
        float const mouse_dots_per_screen_x = 1.0f/2.0f;
        float const mouse_dots_per_screen_y = 1.0f/2.0f;
        
        ctx->quit = false;
        ctx->mouse_input_toggled = false;
        ctx->mouse_delta_screen = {};
        
        int32 mouse_delta_x_dots = 0;
        int32 mouse_delta_y_dots = 0;
            
        process_pending_messages(
            state,
            &ctx->quit,
            &ctx->exit_code,
            &mouse_delta_x_dots,
            &mouse_delta_y_dots,
            &ctx->mouse_input_toggled
            );

        Vec2::set(
            +(float)mouse_delta_x_dots*mouse_dots_per_screen_x,
            -(float)mouse_delta_y_dots*mouse_dots_per_screen_y,
            &ctx->mouse_delta_screen
            );

        if( !ctx->quit )
        {
            if( ctx->mouse_input_toggled )
            {
                state->mouse_input_enabled = !state->mouse_input_enabled;
                bool trap_mouse = state->mouse_input_enabled;
                ShowCursor( !trap_mouse );
                set_mouse_trapping(g_window_handle, trap_mouse);
            }
        }
        
    }

    bool init(uint viewport_width_screen, uint viewport_height_screen, bool mouse_input_initially_enabled)
    {

    
        LPCSTR const window_class_name = "IIR4InputWindowClass";
    
        {
            TIMECAPS time_caps;
            UINT time_caps_size = sizeof(time_caps);
            MMRESULT result = timeGetDevCaps(&time_caps, time_caps_size);
            if( result != MMSYSERR_NOERROR )
            {
                log_line_string("failed to get the timer device capabilities");
                return 0;
            }
            UINT timerdevice_minimum_period_milliseconds = time_caps.wPeriodMin;
            UINT timerdevice_maximum_period_milliseconds = time_caps.wPeriodMax;

            { // log the timer device capabilities
                char message_buffer[512] = {};
            
                _snprintf_s(message_buffer, sizeof(message_buffer),
                            "timer device capabilities: minimum period: %dms, maximum period: %dms\n",
                            timerdevice_minimum_period_milliseconds,
                            timerdevice_maximum_period_milliseconds);
                log_string(message_buffer);
            }
        }

        g_sleep_is_granular = false;
        {
            UINT period = 1;
            MMRESULT result = timeBeginPeriod(period);
            if( result != MMSYSERR_NOERROR)
            {
                log_string("error: failed to set the time begin period");
                // NOTE: this is not a fatal error, just means that sleep is not granular
                // and we will have to spin-lock instead of doing a proper sleep
            }
            else if( result == TIMERR_NOCANDO )
            {
                log_string("did not get the requested time begin period");
                g_sleep_is_granular = false;
            }
            else
            {
                g_sleep_is_granular = true;
            }
        }    
    
        // NOTE:
        // This frequency is consistent across reboots and processors,
        // so it only needs to be queried this one time at startup.
        global_perfcounter_frequency = 0;
        {
            LARGE_INTEGER result;
            QueryPerformanceFrequency(&result);
            global_perfcounter_frequency = result.QuadPart;
        }
        assert( global_perfcounter_frequency > 0 );

        {
            WNDCLASS window_class = {};
            window_class.style = CS_HREDRAW | CS_VREDRAW;
            window_class.lpfnWndProc = window_callback;
            window_class.hInstance = g_instance;
            window_class.hIcon = 0;   // TODO: provide an icon
            window_class.hCursor = 0; // TODO: figure out how to hide cursor
            window_class.lpszClassName = window_class_name;  
            ATOM atom = RegisterClassA(&window_class);
            if( atom == 0 )
            {
                log_string("failed to register window class");
                return 0;
            }
        }
  
        g_window_handle = 0;
        {
            DWORD extended_style = 0;
            LPCTSTR class_name = window_class_name;
            LPCTSTR window_name = "IIR4 Input";
            DWORD style =
                WS_VISIBLE |
                WS_OVERLAPPED |
                WS_CAPTION |
                WS_SYSMENU |
                WS_MINIMIZEBOX |
                WS_MAXIMIZEBOX;
            int x = CW_USEDEFAULT;
            int y = CW_USEDEFAULT;
            // NOTE: top-level window, so no parent
            HWND parent_window = 0;
            HMENU menu_handle = 0;
            // NOTE: no additional data needed
            LPVOID lparam  = 0;

            // NOTE:
            // Here, we figure out what _window_ width and height should be, in order to get a
            // desired _client rectangle_ width and height.
            int width = 0;
            int height = 0;
            {
                RECT desired_client_rectangle = {};            
                desired_client_rectangle.left = 0;
                desired_client_rectangle.top = 0;
                desired_client_rectangle.right = viewport_width_screen;
                desired_client_rectangle.bottom = viewport_height_screen;
                BOOL has_menu = FALSE;
                BOOL success = AdjustWindowRectEx(&desired_client_rectangle,
                                                  style,
                                                  has_menu,
                                                  extended_style);
                if( success == FALSE )
                {
                    log_string("failed to adjust the window rectangle to fit client rectangle dimensions");
#if IIR4_INPUT_BUILDTYPE == IIR4_INPUT_BUILDTYPE_INTERNAL
                    return 0;
#else
                    width = CW_USEDEFAULT;
                    height = CW_USEDEFAULT;
#endif          
                }
                else
                {
                    width = desired_client_rectangle.right - desired_client_rectangle.left;
                    assert( width >= 0 );
                    height = desired_client_rectangle.bottom - desired_client_rectangle.top;
                    assert( height >= 0 );
                }
            }        
        
            g_window_handle =
                CreateWindowEx(
                    extended_style,
                    class_name,
                    window_name,
                    style,
                    x,
                    y,
                    width,
                    height,
                    parent_window,
                    menu_handle,
                    g_instance,
                    lparam);

            if( g_window_handle == 0 )
            {
                log_string("failed to create window");
                return 0;
            }
        }
        assert(g_window_handle != 0);

        { // register for raw input from the mouse
            RAWINPUTDEVICE device = {};
            // NOTE:
            // According to "Special Case Hardware IDs for Internal Use Only"
            // (https://msdn.microsoft.com/en-us/library/ff543440.aspx),
            // the usage and usage page page for a 'Mouse' device type is 0x01 and 0x02, respectively.
            device.usUsagePage = 0x01;
            device.usUsage = 0x02;
            // STUDY: There is a RIDEV_CAPTUREMOUSE flag. Find out what it does!
            // NOTE: It seems that using RIDEV_NOLEGACY in windowed mode means windows doesn't
            // handle window dragging for us.
            device.dwFlags = 0;
            device.hwndTarget = 0; // NULL means follow window focus

            PCRAWINPUTDEVICE devices = &device;
            UINT num_devices = 1;
            UINT size = sizeof(RAWINPUTDEVICE);

            BOOL success =
                RegisterRawInputDevices(
                    devices,
                    num_devices,
                    size
                    );
        
            if(!success)
            {
                log_line_string("failed to register raw mouse input device");
                return 0;
            }
        
        }
            

        { // set initial mouse mode
            bool trap_mouse = mouse_input_initially_enabled;

            // NOTE: calling ShowCursor(true) will increment a counter which is 1 initially.
            // We don't want this counter to go to 2 since we want to be able to toggle it easily below.
            if( trap_mouse )
                ShowCursor( false );
        
            set_mouse_trapping(g_window_handle, trap_mouse);
        }
            
        return true;
    }
    
};
