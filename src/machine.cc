#include <iostream>
/**
 * Основной цикл работы VM
 */
void Z80Spectrum::main() {

#ifndef NO_SDL
    // Инициализация SDL
    if (sdl_enable) {        
        SDL_Init(SDL_INIT_EVERYTHING);
        //SDL_EnableUNICODE(1);

        //sdl_screen = SDL_SetVideoMode(3*320, 3*240, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);        
        //SDL_WM_SetCaption("ZX Spectrum Virtual Machine", 0);
        sdl_screen = SDL_CreateWindow("ZX Spectrum Virtual Machine",
                        100,
                        100,
                        3*320, 3*240,
                        SDL_WINDOW_SHOWN
                    );
        sdl_renderer = SDL_CreateRenderer(sdl_screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
        sdl_texture = SDL_CreateTexture(sdl_renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               3*320, 3*240);
        pixels = (Uint32*) malloc(4*3*320*3*240);
        //SDL_EnableKeyRepeat(500, 30);

        // Количество семплов 882 x 50 = 44100
        if (sdl_disable_sound == 0) {

            audio_device.freq     = 44100;
            audio_device.format   = AUDIO_U8;
            audio_device.channels = 2;
            audio_device.samples  = 882; // два канала, #DONE: надо переделать 
            audio_device.callback = sdl_audio_buffer;
            audio_device.userdata = NULL;          

            if (SDL_OpenAudio(&audio_device, NULL) < 0 ) {
            //if (SDL_OpenAudioDevice(NULL, 0, &audio_device, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE) <0) {
                fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());                
                exit(1);
            }
            printf("Silence value:%d Buffer size:%d Format:%d\n",
                     audio_device.silence, audio_device.size, audio_device.format);

            for (int w = 0; w < MAX_AUDIOSDL_BUFFER; w++) ZXAudioBuffer[w] = 0x80; //тишина!

            SDL_PauseAudio(0);
        }

        // Если при старте включен отладчик - перерисовать его окно
        if (ds_viewmode == 0) { ds_cursor = ds_start = pc; disasm_repaint(); }
        int time_curr = SDL_GetTicks();
        while (1) {

            // Регистрация событий
            while (SDL_PollEvent(&event)) {

                switch (event.type) {

                    case SDL_QUIT:

                        // Выдать статистику использования опкодов
                        // for (int i = 0; i < 256; i++) printf("%08x %x \n", statistics[i], i);
                        free(pixels);
                        SDL_CloseAudio();
                        return;

                    case SDL_KEYDOWN: keyb(1, &event.key); break;
                    case SDL_KEYUP:   keyb(0, &event.key); break;
                }
            }

            // Вычисление разности времени
            //ftime(&ms_clock);
            //int time_curr = ms_clock.millitm;
            //int time_diff = time_curr - ms_clock_old;
            int time_diff = SDL_GetTicks() - time_curr;
            //if (time_diff < 0) time_diff += 1000;

            // Если прошло 20 мс
            if ( time_diff > 19) { // 50Гц
                time_curr += time_diff; 
                if (ds_viewmode) frame();
                //ms_clock_old = time_curr;                
                //SDL_Flip(sdl_screen);
                SDL_UpdateTexture(sdl_texture, NULL, pixels, 3*320 * sizeof(Uint32)); // ширина строки
                SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
                SDL_RenderPresent(sdl_renderer);    
                //time_diff = SDL_GetTicks() - time_curr-19;
                //if (time_diff>0) printf("%d-", time_diff);
            }

            // SDL_PumpEvents();
            // SDL_Delay(1);
        }
    }
    // Выполнение спектрума из консоли
    else
#endif
    {
        if (con_frame_end == 0) con_frame_end = 150; // 3 sec
        while (frame_counter < con_frame_end) frame();
    }
}

// Разбор аргументов
void Z80Spectrum::args(int argc, char** argv) {
    
    int bin_offset;    
    for (int u = 1; u < argc; u++) {
        printf("argc=%d, arg=%s \r\n", u, argv[u]);
        // Параметр
        if (argv[u][0] == '-') {

            switch (argv[u][1]) {

                // 128k режим
                case '2': port_7ffd = 0; break;

                // Включение последовательности автостарта (RUN ENT)
                case 'a': autostart = 1; break;

                // Загрузка бинарного файла
                case 'b':

                    sscanf(argv[u+2], "%x", &bin_offset);
                    loadbin(argv[u+1], bin_offset);
                    u += 2;
                    break;

                // Отключение SDL
                case 'c': sdl_enable = 0; break;

                // При загрузке включить отладчик
                case 'd': ds_viewmode = 0; break;

                // Остановка на HALT
                case 'h': ds_halt_dump = 1; break;

                // Нажатие на пробел через некоторое время
                case 'k': auto_keyb = 1; break;

                // Пропуск кадров
                case 'm':

                    sscanf(argv[u+1], "%d", &skip_first_frames); u++;
                    break;

                // Длительность
                case 'M':

                    sscanf(argv[u+1], "%d", &con_frame_end); u++;
                    con_frame_end *= 50;
                    break;

                // Файл для записи видео
                case 'o':

                    if (strcmp(argv[u+1],"-") == 0) {
                        record_file = stdout;
                    } else {
                        record_file = fopen(argv[u+1], "w+");
                    }
                    u++;
                    break;

                // Установка регистра PC (hex)
                case 'p':

                    sscanf(argv[u+1], "%x", & pc); u++;
                    break;

                // Загрузка ROM 0/1
                case 'r':

                    loadrom(argv[u+1], argv[u][2] - '0'); u++;
                    break;

                // Скип дублирующийся фреймов
                case 's': skip_dup_frame = 1; break;

                // Файл для записи звука
                case 'w':

                    wave_file = fopen(argv[u+1], "wb");
                    if (wave_file == NULL) { printf("Can't open file %s for writing\n", argv[u+1]); exit(1); }
                    fseek(wave_file, 44, SEEK_SET);
                    u++;
                    break;

                // Отключение звука
                case 'x': sdl_disable_sound = 1; break;

                // Активация моно
                case 'z': ay_mono = 1; break;
            }

        }
        // Загрузка файла
        else if (strstr(argv[u], ".z80") != NULL) {
            loadz80(argv[u]);
        }
        // Загрузка файла BAS с ленты
        else if (strstr(argv[u], ".tap") != NULL) {
            loadtap(argv[u]);
        }
        // Загрузка файла SNA
        else if (strstr(argv[u], ".sna") != NULL) {
            loadsna(argv[u]);
        }
    }
}

// Симулятор автоматического нажатия на клавиши
void Z80Spectrum::autostart_macro() {

    autostart++;
    switch (autostart) {

        case 1: autostart = 0; break;
        // Выполнение макроса RUN
        case 2: key_press(2, 0x08, 1); break; // R
        case 3: key_press(2, 0x08, 0); break;
        case 4: key_press(6, 0x01, 1); break; // ENT
        case 5: key_press(6, 0x01, 0); break;
        // Выключить или продолжать?
        case 6: autostart = 0; break;
    }

    // Симулятор нажатия на кнопки (SPACE)
    if (auto_keyb) {

        switch (frame_id) {

            case 25: key_press(7, 0x01, 1); break;
            case 26: key_press(7, 0x01, 0); break;
        }
    }

    frame_id++;
}

// Занесение нажатия в регистры
void Z80Spectrum::key_press(int row, int mask, int press) {

    if (press) {
        key_states[ row ] &= ~mask;
    } else {
        key_states[ row ] |=  mask;
    }
}

#ifndef NO_SDL
// Нажатие клавиши SDL
// https://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlkey.html
void Z80Spectrum::keyb(int press, SDL_KeyboardEvent* eventkey) {

    int key = eventkey->keysym.sym;
    int ts;

    inreg = 0;

    switch (key) {

        // Первый ряд
        case SDLK_1: key_press(3, 0x01, press); inreg = kshift ? '!' : '1'; break;
        case SDLK_2: key_press(3, 0x02, press); inreg = kshift ? '@' : '2'; break;
        case SDLK_3: key_press(3, 0x04, press); inreg = kshift ? '#' : '3'; break;
        case SDLK_4: key_press(3, 0x08, press); inreg = kshift ? '$' : '4'; break;
        case SDLK_5: key_press(3, 0x10, press); inreg = kshift ? '%' : '5'; break;
        case SDLK_6: key_press(4, 0x10, press); inreg = kshift ? '^' : '6'; break;
        case SDLK_7: key_press(4, 0x08, press); inreg = kshift ? '&' : '7'; break;
        case SDLK_8: key_press(4, 0x04, press); inreg = kshift ? '*' : '8'; break;
        case SDLK_9: key_press(4, 0x02, press); inreg = kshift ? '(' : '9'; break;
        case SDLK_0: key_press(4, 0x01, press); inreg = kshift ? ')' : '0'; break;

        // Второй ряд
        case SDLK_q: key_press(2, 0x01, press); inreg = kshift ? 'Q' : 'q'; break;
        case SDLK_w: key_press(2, 0x02, press); inreg = kshift ? 'W' : 'w'; break;
        case SDLK_e: key_press(2, 0x04, press); inreg = kshift ? 'E' : 'e'; break;
        case SDLK_r: key_press(2, 0x08, press); inreg = kshift ? 'R' : 'r'; break;
        case SDLK_t: key_press(2, 0x10, press); inreg = kshift ? 'T' : 't'; break;
        case SDLK_y: key_press(5, 0x10, press); inreg = kshift ? 'Y' : 'y'; break;
        case SDLK_u: key_press(5, 0x08, press); inreg = kshift ? 'U' : 'u'; break;
        case SDLK_i: key_press(5, 0x04, press); inreg = kshift ? 'I' : 'i'; break;
        case SDLK_o: key_press(5, 0x02, press); inreg = kshift ? 'O' : 'o'; break;
        case SDLK_p: key_press(5, 0x01, press); inreg = kshift ? 'P' : 'p'; break;

        // Третий ряд
        case SDLK_a: key_press(1, 0x01, press); inreg = kshift ? 'A' : 'a'; break;
        case SDLK_s: key_press(1, 0x02, press); inreg = kshift ? 'S' : 's'; break;
        case SDLK_d: key_press(1, 0x04, press); inreg = kshift ? 'D' : 'd'; break;
        case SDLK_f: key_press(1, 0x08, press); inreg = kshift ? 'F' : 'f'; break;
        case SDLK_g: key_press(1, 0x10, press); inreg = kshift ? 'G' : 'g'; break;
        case SDLK_h: key_press(6, 0x10, press); inreg = kshift ? 'H' : 'h'; break;
        case SDLK_j: key_press(6, 0x08, press); inreg = kshift ? 'J' : 'j'; break;
        case SDLK_k: key_press(6, 0x04, press); inreg = kshift ? 'K' : 'k'; break;
        case SDLK_l: key_press(6, 0x02, press); inreg = kshift ? 'L' : 'l'; break;
        case SDLK_RETURN:    key_press(6, 0x01, press); inreg = 10; break;
        case SDLK_KP_ENTER:  key_press(6, 0x01, press); inreg = 10; break;

        // Четвертый ряд
        case SDLK_LSHIFT:   key_press(0, 0x01, press); kshift = press; break;
        case SDLK_z:        key_press(0, 0x02, press); inreg = kshift ? 'Z' : 'z'; break;
        case SDLK_x:        key_press(0, 0x04, press); inreg = kshift ? 'X' : 'x'; break;
        case SDLK_c:        key_press(0, 0x08, press); inreg = kshift ? 'C' : 'c'; break;
        case SDLK_v:        key_press(0, 0x10, press); inreg = kshift ? 'V' : 'v'; break;
        case SDLK_b:        key_press(7, 0x10, press); inreg = kshift ? 'B' : 'b'; break;
        case SDLK_n:        key_press(7, 0x08, press); inreg = kshift ? 'N' : 'n'; break;
        case SDLK_m:        key_press(7, 0x04, press); inreg = kshift ? 'M' : 'm'; break;
        case SDLK_RSHIFT:   key_press(7, 0x02, press); kshift = press; break;
        case SDLK_SPACE:    key_press(7, 0x01, press); inreg = ' '; break;

        // Специальные
        case SDLK_UP:           key_press(0, 0x01, press); key_press(4, 0x08, press); inreg = 1; break; // SS+7
        case SDLK_RIGHT:        key_press(0, 0x01, press); key_press(4, 0x04, press); inreg = 2; break; // SS+8
        case SDLK_DOWN:         key_press(0, 0x01, press); key_press(4, 0x10, press); inreg = 3; break; // SS+6
        case SDLK_LEFT:         key_press(0, 0x01, press); key_press(3, 0x10, press); inreg = 4; break; // SS+5
        case SDLK_TAB:          key_press(0, 0x01, press); key_press(7, 0x02, press); inreg = 9; break; //  SS+CS
        case SDLK_CAPSLOCK:     key_press(0, 0x01, press); key_press(3, 0x02, press); break; // SS+2 CAP (DANGER)
        case SDLK_BACKSPACE:    key_press(0, 0x01, press); key_press(4, 0x01, press); inreg = 8; break; // SS+0 BS
        case SDLK_ESCAPE:       key_press(0, 0x01, press); key_press(7, 0x01, press); inreg = 27; break; // SS+SPC
        case SDLK_BACKQUOTE:    key_press(0, 0x01, press); key_press(3, 0x01, press); inreg = kshift ? '~' : '`'; break; // SS+1 EDIT
        case SDLK_MINUS:        key_press(7, 0x02, press); key_press(6, 0x08, press); inreg = kshift ? '_' : '-'; break; // -
        case SDLK_EQUALS:       key_press(7, 0x02, press); key_press(6, 0x02, press); inreg = kshift ? '+' : '='; break; // =
        case SDLK_COMMA:        key_press(7, 0x02, press); key_press(7, 0x08, press); inreg = kshift ? '<' : ','; break; // ,
        case SDLK_PERIOD:       key_press(7, 0x02, press); key_press(7, 0x04, press); inreg = kshift ? '>' : '.'; break; // .

        // KeyPad
        case SDLK_KP_PLUS:      key_press(7, 0x02, press); key_press(6, 0x04, press); inreg = '+'; break; // +
        case SDLK_KP_MINUS:     key_press(7, 0x02, press); key_press(6, 0x08, press); inreg = '-'; break; // -
        case SDLK_KP_MULTIPLY:  key_press(7, 0x02, press); key_press(7, 0x10, press); inreg = '*'; break; // *
        case SDLK_KP_DIVIDE:    key_press(7, 0x02, press); key_press(0, 0x10, press); inreg = '/'; break; // /

        // Дополнительные
        case SDLK_BACKSLASH:    inreg = kshift ? '|' : '\\'; break;
        case SDLK_LEFTBRACKET:  inreg = kshift ? '{' :'['; break;
        case SDLK_RIGHTBRACKET: inreg = kshift ? '}' :']'; break;
        case SDLK_SEMICOLON:    inreg = kshift ? ':' :';'; break;
        case SDLK_QUOTE:        inreg = kshift ? '"' :'\''; break;
        case SDLK_SLASH:        inreg = kshift ? '?' :'/'; break;

        // Отладка
        default:

            if (press)
            switch (key) {

                case SDLK_F2: savesna("autosave.sna"); break;
                case SDLK_F3: loadsna("autosave.sna"); break;
                case SDLK_F4: 
                    if (tapsize>0) {
                        start_tape = !start_tape; if (start_tape) initTape();
                    } else {
                        printf("Tap file is not loaded!\n");    
                    }
                    break;

                // Показать/скрыть экран
                case SDLK_F5: if (ds_showfb) disasm_repaint(); else redraw_fb(); break;

                // Пошаговое исполнение
                case SDLK_F7:
                    printf("step...\r\n");
                    if (ds_viewmode) {

                        ds_viewmode = 0;
                        ds_cursor   = ds_start = pc;

                    } else {

                        halted = 0;

                        ts = run_instruction();
                        t_states_cycle += ts;
                        t_states_all   += ts;
                        ds_cursor = pc;
                    }

                    disasm_repaint();
                    break;

                // Запуск программы
                case SDLK_F9:

                    if (ds_viewmode == 0) ds_viewmode = 1;
                    break;

                case SDLK_F10: 
                    loadbin("zexall", 0x8000); 
                    printf("zexall is loaded\r\n");
                    break;
            }
    }

    if (inreg && press) klatch ^= 1;
}
#endif
