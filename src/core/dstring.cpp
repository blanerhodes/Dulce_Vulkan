
u32 StringLength(char* str) {
    u32 result = 0;

    while (*str) {
        result++;
        str++;
    }
    result++;
    return result;
}

static b8 StringsEqual(char* str1, char* str2) {
    if (!str1 || !str2) {
        DERROR("StringsEqual - Null string passed.");
        return false;
    }
    while ((*str1 && *str2) && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return (*str1 == '\0' && *str2 == '\0');
}

static b8 StringsEquali(char* str1, char* str2) {
    if (!str1 || !str2) {
        DERROR("StringsEquali - Null string passed.")
    }
    u32 lowercase_offset = 'a' - 'A';
    while (*str1 && *str2) {
        char char1 = *str1;
        char char2 = *str2;
        if (char1 >= 'A' && char1 <= 'Z') {
            char1 += lowercase_offset;
        }
        if (char2 >= 'A' && char2 <= 'Z') {
            char2 += lowercase_offset;
        }
        if (char1 != char2) {
            break;
        }
        str1++;
        str2++;
    }
    return (*str1 == '\0' && *str2 == '\0');
}