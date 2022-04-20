internal bool
String_Match(String s0, String s1)
{
    return (s0.size == s1.size && MemMatch(s0.data, s1.data, s0.size));
}

internal umm
String_Length(char* cstring)
{
    umm length = 0;
    while (*cstring != 0) ++cstring, ++length;
    return length;
}

internal imm
String_FormatArgList(Buffer buffer, char* format, va_list arg_list)
{
    umm req_chars = 0;
    
    for (char* scan = format; *scan != 0; ++scan)
    {
        if (*scan != '%')
        {
            if (req_chars + 1 <= buffer.size) buffer.data[req_chars] = *scan;
            req_chars += 1;
        }
        else
        {
            ++scan;
            if (*scan == '%')
            {
                if (req_chars + 1 <= buffer.size) buffer.data[req_chars] = *scan;
                req_chars += 1;
            }
            else if (*scan == 's')
            {
                char* arg = va_arg(arg_list, char*);
                
                umm length = String_Length(arg);
                
                if (req_chars < buffer.size)
                {
                    Copy(arg, buffer.data + req_chars, (req_chars + length <= buffer.size ? length : buffer.size - req_chars));
                }
                req_chars += length;
            }
            else if (*scan == 'S')
            {
                String arg = va_arg(arg_list, String);
                
                if (req_chars < buffer.size)
                {
                    Copy(arg.data, buffer.data + req_chars, (req_chars + arg.size <= buffer.size ? arg.size : buffer.size - req_chars));
                }
                req_chars += arg.size;
            }
            else if (*scan == 'u' || *scan == 'U' ||
                     *scan == 'i' || *scan == 'I')
            {
                umm num;
                if      (*scan == 'u') num = va_arg(arg_list, u32);
                else if (*scan == 'U') num = va_arg(arg_list, u64);
                else
                {
                    imm inum;
                    
                    if (*scan == 'i') inum = va_arg(arg_list, i32);
                    else              inum = va_arg(arg_list, i64);
                    
                    if (inum < 0)
                    {
                        inum = -inum;
                        if (req_chars < buffer.size) buffer.data[req_chars] = '-';
                        req_chars += 1;
                    }
                    
                    num = (umm)inum;
                }
                
                umm largest_place = 1;
                for (umm cpy = num / 10; cpy != 0; cpy /= 10) largest_place *= 10;
                
                while (largest_place != 0)
                {
                    if (req_chars < buffer.size) buffer.data[req_chars] = (u8)((num / largest_place) % 10 + '0');
                    largest_place /= 10;
                    req_chars     += 1;
                }
            }
            else if (*scan == 'x' || *scan == 'X')
            {
                umm num;
                if (*scan == 'x') num = va_arg(arg_list, u32);
                else              num = va_arg(arg_list, u64);
                
                if (req_chars + 0 < buffer.size) buffer.data[req_chars + 0] = '0';
                if (req_chars + 1 < buffer.size) buffer.data[req_chars + 1] = 'x';
                req_chars += 2;
                
                do
                {
                    char c = num % 16;
                    if (c >= 10) c  = (c - 10) + 'A';
                    else         c += '0';
                    
                    if (req_chars < buffer.size) buffer.data[req_chars] = c;
                    req_chars += 1;
                    num       /= 16;
                } while (num != 0);
            }
            else if (*scan == 'f')
            {
                NOT_IMPLEMENTED;
            }
            else
            {
                if (req_chars + 3 <= buffer.size) Copy("[?]", buffer.data + req_chars, 3);
                req_chars += 3;
            }
        }
    }
    
    if (req_chars < buffer.size) buffer.data[req_chars++] = 0;
    else
    {
        if (buffer.size >= sizeof("...")) Copy("...", buffer.data + buffer.size - sizeof("..."), sizeof("..."));
        else if (buffer.size > 0) buffer.data[buffer.size - 1] = 0;
    }
    
    return (req_chars > buffer.size ? -(imm)req_chars : (imm)req_chars);
}