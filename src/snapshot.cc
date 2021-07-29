// Раскидывает банки памяти для снапшота
int Z80Spectrum::z80file_bankmap(int mode, int bank) {

    // page0..7
    if (mode == 3 || mode == 4) {
        return bank - 3;
    }
    else if (mode == 0 || mode == 1) {

        switch (bank) {

            case 4: return 2; // 8000-bfff
            case 5: return 0; // c000-ffff
            case 8: return 5; // 4000-7fff
            default:

                printf("Z80 loader: can't recognize bank %d for hmode=%d ", bank, mode);
                exit(1);
        }
    }

    return 0;
}

// Для нормальной загрузки 48k z80 снапшотов (mode=1)
int Z80Spectrum::c48k_address(int address, int mode) {

    if (mode) {

        switch (address & 0xc000) {

            case 0x4000: return (address&0x3fff) + 5*0x4000; break;
            case 0x8000: return (address&0x3fff) + 2*0x4000; break;
            case 0xc000: return (address&0x3fff) + 0*0x4000; break;
        }
    }

    return address;
}

// Загрузка бинарника
void Z80Spectrum::loadbin(const char* filename, int address) {

    unsigned char databin[64*1024];

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) { printf("BINARY %s not exists\n", filename); exit(1); }
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(databin, 1, fsize, fp);
    fclose(fp);

    for (int w = 0; w < fsize; w++) mem_write(address + w, databin[w]);
}

// Загрузка базового ROM
void Z80Spectrum::loadrom(const char* filename, int bank) {

    char fn[128];

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {

        sprintf(fn, "/usr/local/share/vmzx/%s", filename);
        fp = fopen(fn, "rb");
        if (fp == NULL) {
            printf("ROM %s not exists\n", filename);
            exit(1);
        }
    }

    if (bank < 4) {
        fread(rom + 16384*bank, 1, 16384, fp);
    } else {
        fread(trdos, 1, 16384, fp);
    }
    fclose(fp);
}

// Загрузка снапшота
// https://worldofspectrum.org/faq/reference/z80format.htm
// https://worldofspectrum.org/faq/reference/128kreference.htm
void Z80Spectrum::loadz80(const char* filename) {

    char fn[128];
    unsigned char data[128*1024];

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {

        sprintf(fn, "/usr/local/share/vmzx/%s", filename);
        fp = fopen(fn, "rb");
        if (fp == NULL) {
            printf("Can't load file %s\n", filename);
            exit(1);
        }
    }

    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(data, 1, fsize, fp);
    fclose(fp);

    // Установка регистров
    a  = data[0];
    set_flags_register(data[1]);
    c  = data[2];
    b  = data[3];
    l  = data[4];
    h  = data[5];
    pc = data[6] + 256*data[7];
    sp = data[8] + 256*data[9];
    i  = data[10];
    r  = data[11];
    e  = data[13];
    d  = data[14];

    c_prime = data[15];
    b_prime = data[16];
    e_prime = data[17];
    d_prime = data[18];
    l_prime = data[19];
    h_prime = data[20];
    a_prime = data[21];
    set_flags_prime(data[22]);

    iy      = data[23] + 256*data[24];
    ix      = data[25] + 256*data[26];
    iff1    = data[27] ? 1 : 0;
    iff2    = data[28] ? 1 : 0;
    imode   = data[29] & 3;

    // старший бит R
    r |= ((data[12] & 1) << 7);

    // цвет бордюда
    io_write(0xFE, (data[12] & 0x0E) >> 1);

    int rle     = (data[12] & 0x20) == 0x20 ? 1 : 0;
    int address = 0x4000;
    int cursor  = 30;
    int version = 1;

    if (pc == 0) {

        int _len   = data[30] + 256*data[31];
        int _hmode = data[34];

        pc        = data[32] + 256*data[33];
        port_7ffd = data[35]; // 128k режим

        io_write(0x7ffd, port_7ffd);

        // AY данные
        ay_register = data[38];
        for (int _a = 0; _a < 16; _a++) ay_regs[_a] = data[39+_a];

        if (_len == 23)      { cursor = 55; version = 2; }
        else if (_len == 54) { cursor = 86; version = 3; }
        else if (_len == 55) { cursor = 87; version = 3; }

        if (version > 2) {

            //sprintf(strbuf, "ZXCORE: Z80 file has %d version\n", version);
            //fputs(strbuf, stderr);
            //exit(1);
        }

        // Поддерживаемые режимы: 3,4(128k) 0,1(48k)
        if (_hmode != 0 && _hmode != 1 && _hmode != 3 && _hmode != 4) {

            sprintf(strbuf, "ZXCORE: Hardware mode is not 128k (mode=%d, pc=%x, cursor=%x)\n", _hmode, pc, cursor);
            fputs(strbuf, stderr);
            exit(1);
        }

        // Для 48k будет всегда регистр памяти равен 10h
        if (_hmode < 2) port_7ffd = 0x0010;

        // Следующий блок
        while (cursor < fsize) {

            int data_size = data[cursor] + data[cursor+1]*256;

            // Для 128k данные в [3..11]
            int data_bank = z80file_bankmap(_hmode, data[cursor + 2]);

            // Переход к данным
            cursor += 3;

            // Не скомпрессированные данные
            if (data_size == 0xffff) { rle = 0; data_size = 0x4000; } else rle = 1;

            // Вычислить следующий адрес
            address = (data_bank) * 0x4000;

            // Явно указать окончание данных
            int next = cursor + data_size;
            if (next > fsize) next = fsize;

            // Загрузка блока
            loadz80block(0, cursor, address, data, cursor + data_size, rle);
        }
    }
    // Обычная загрузка блока 48к (v1)
    else {

        port_7ffd = 0x30;
        loadz80block(1, cursor, address, data, fsize, rle);
    }
}

// Загрузка блока в память
void Z80Spectrum::loadz80block(int mode, int& cursor, int &addr, unsigned char* data, int top, int rle) {

    if (rle) {

        while (cursor < top) {

            // EOF
            if (data[cursor]   == 0x00 &&
                data[cursor+1] == 0xED &&
                data[cursor+2] == 0xED &&
                data[cursor+3] == 0x00) {
                break;
            }

            // Процедура декомпрессии
            if (data[cursor]   == 0xED &&
                data[cursor+1] == 0xED) {

                for (int t_ = 0; t_ < data[cursor+2]; t_++) {

                    memory[c48k_address(addr, mode)] = data[cursor+3];
                    addr++;
                }

                cursor += 4;

            } else {

                memory[c48k_address(addr, mode)] = data[cursor];

                addr++;
                cursor++;
            }
        }

    } else {

        while (cursor < top) {

            memory[c48k_address(addr, mode)] = data[cursor];
            addr++;
            cursor++;
        }
    }
}

// https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
void Z80Spectrum::loadtap(const char* filename) {

    unsigned char tapfile[64*1024];

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) { printf("No file %s\n", filename); exit(1); }
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(tapfile, 1, fsize, fp);
    fclose(fp);

    // Первым в TAP должен идти бейсик
    if (tapfile[0x17] != 0xFF) {
        printf("No BASIC program\n"); exit(1);
    }

    // Размер бейсик-программы
    int bsize = tapfile[0x15] + tapfile[0x16]*256 - 2;

    // Запись в программную область
    for (int q = 0; q < bsize; q++) memory[c48k_address(0x5ccb+q, 1)] = tapfile[0x18 + q];

    int endp = 0x5ccb + bsize;
    int next = endp;

    // END OF PROGRAM
    put48mem_word(endp,   0x0D80);  // Линия 1
    put48mem_word(endp+2, 0x2280);
    put48mem_word(endp+4, 0x800D);  // Линия 2
    put48mem_word(0x5C4B, next);    // VARS :: https://skoolkid.github.io/rom/asm/5C4B.html
    next++;

    put48mem_word(0x5C59, next);    // E-LINE :: https://skoolkid.github.io/rom/asm/5C59.html
    put48mem_word(0x5C5B, next);    // K-CUR :: https://skoolkid.github.io/rom/asm/5C5B.html

    next+=2;
    put48mem_word(0x5C61, next);    // WORKSP :: https://skoolkid.github.io/rom/asm/5C61.html
    put48mem_word(0x5C63, next);    // STKBOT :: https://skoolkid.github.io/rom/asm/5C63.html
    put48mem_word(0x5C65, next);    // STKEND :: https://skoolkid.github.io/rom/asm/5C65.html

    next++;
    put48mem_word(0x5C5D, next);    // CH-ADD :: https://skoolkid.github.io/rom/asm/5C5D.html

    next++;
    put48mem_word(0x5C55, next);    // NXTLIN :: https://skoolkid.github.io/rom/asm/5C55.html

/*
    // Всякие непонятные параметры
    put48mem_byte(0x5C3B, 0x84);
    put48mem_byte(0x5C5F, 0xF8);
    put48mem_byte(0x5C67, 0x1B); //  CALC B reg
    *
    put48mem_word(0x5C44, 0xFEFF);
    put48mem_word(0x5C46, 0x01FF);
    put48mem_word(0x5C74, 0x1A01);
*/
}

// Сохранение снапшота в файл (не RLE) 48k
void Z80Spectrum::savez80(const char* filename) {

    unsigned char data[48*1024];

    FILE* fp = fopen(filename, "wb+");

    // Установка регистров
    data[0] = a;
    data[1] = get_flags_register();
    data[2] = c;
    data[3] = b;
    data[4] = l;
    data[5] = h;
    data[6] = pc & 0xff;
    data[7] = (pc >> 8) & 0xff;
    data[8] = sp & 0xff;
    data[9] = (sp >> 8) & 0xff;
    data[10] = i;
    data[11] = (r & 0x7f);
    data[12] = (r >> 7) | ((border_id&7)<<1) | 0x00; // старший бит R, RLE=0
    data[13] = e;
    data[14] = d;
    data[15] = c_prime;
    data[16] = b_prime;
    data[17] = e_prime;
    data[18] = d_prime;
    data[19] = l_prime;
    data[20] = h_prime;
    data[21] = a_prime;
    data[22] = get_flags_prime();

    data[23] = iy & 0xff;
    data[24] = (iy >> 8) & 0xff;

    data[25] = ix & 0xff;
    data[26] = (ix >> 8) & 0xff;

    data[27] = iff1 ? 1 : 0;
    data[28] = iff2 ? 1 : 0;
    data[29] = imode;

    fwrite(data, 1, 30, fp);

    // Преобразовать память
    for (int _a = 0x4000; _a < 0x10000; _a++)
        data[_a - 0x4000] = memory[c48k_address(_a, 1)];

    fwrite(data, 1, 0xc000, fp);
    fclose(fp);
}

// Загрука файла снапшота
// http://speccy.info/SNA
void Z80Spectrum::loadsna(const char* filename) {

    unsigned char data[192*1024];

    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) { printf("Can't load file %s\n", filename); exit(1); }
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(data, 1, fsize, fp);
    fclose(fp);

    // Базовые параметры
    i = data[0];
    r = data[20];

    l_prime = data[1]; l = data[9];
    h_prime = data[2]; h = data[10];
    e_prime = data[3]; e = data[11];
    d_prime = data[4]; d = data[12];
    c_prime = data[5]; c = data[13];
    b_prime = data[6]; b = data[14];
    set_flags_prime(data[7]);
    a_prime = data[8];

    iy = data[15] + data[16]*256;
    ix = data[17] + data[18]*256;
    sp = data[23] + data[24]*256;

    iff1 = !!(data[19] & 1);
    iff2 = !!(data[19] & 2);

    set_flags_register(data[21]);
    a = data[22];

    imode     = data[25] & 3;
    border_id = data[26] & 7;

    // 48k
    if (fsize == 49179) {

        for (int w = 0; w < 49152; w++)
            memory[ c48k_address(0x4000 + w, 1) ] = data[27 + w];

        pc = pop_word();
    }
    // 128k
    else if (fsize == 131103) {

        pc = data[49179] + 256*data[49180];

        port_7ffd   =   data[49181];
        trdos_latch = !!data[49182];

        io_write(0x7ffd, port_7ffd);

        int sel_bank = port_7ffd & 7;

        for (int w = 0; w < 16384; w++) {

            memory[5*0x4000 + w] = data[27 + w];
            memory[2*0x4000 + w] = data[16411 + w];
            memory[sel_bank*0x4000 + w] = data[32795 + w];
        }

        int _start = 49183;
        for (int n = 0; n < 8; n++) {

            if (n == 2 || n == 5 || n == sel_bank)
                continue;

            for (int w = 0; w < 16384; w++)
                memory[n*0x4000 + w] = data[_start++];
        }
    }
    else if (fsize == 147487) {
        printf("Snapshot 128k+ currently not supported\n");
    }
    else {
        printf("Error snapshot size %d\n", fsize);
        exit(1);
    }
}

// Сохранение 128к снапшота
void Z80Spectrum::savesna(const char* filename) {

    unsigned char data[131103];

    // Базовые параметры
    data[0] = i;
    data[1] = l_prime; data[9]  = l;
    data[2] = h_prime; data[10] = h;
    data[3] = e_prime; data[11] = e;
    data[4] = d_prime; data[12] = d;
    data[5] = c_prime; data[13] = c;
    data[6] = b_prime; data[14] = b;
    data[7] = get_flags_prime();
    data[8] = a_prime;

    data[15] = iy & 0xff; data[16] = (iy>>8) & 0xff;
    data[17] = ix & 0xff; data[18] = (ix>>8) & 0xff;
    data[23] = sp & 0xff; data[24] = (sp>>8) & 0xff;

    data[19] = (iff1&1) | ((iff2&1)<<1);
    data[20] = r;
    data[21] = get_flags_register();
    data[22] = a;

    data[25] = imode & 3;
    data[26] = border_id & 7;

    data[49179] = pc & 0xff;
    data[49180] = (pc>>8) & 0xff;
    data[49181] = port_7ffd;
    data[49182] = !!trdos_latch;

    int sel_bank = port_7ffd & 7;
    for (int w = 0; w < 16384; w++) {

        data[27    + w] = memory[5*0x4000 + w];
        data[16411 + w] = memory[2*0x4000 + w];
        data[32795 + w] = memory[sel_bank*0x4000 + w];
    }

    int _start = 49183;
    for (int n = 0; n < 8; n++) {

        if (n == 2 || n == 5 || n == sel_bank)
            continue;

        for (int w = 0; w < 16384; w++) {
            data[_start++] = memory[n*0x4000 + w];
        }
    }

    FILE* fp = fopen(filename, "w+");
    if (fp == NULL) { printf("Can't write file %s\n", filename); exit(1); }
    fwrite(data, 1, 131103, fp);
    fclose(fp);
}
