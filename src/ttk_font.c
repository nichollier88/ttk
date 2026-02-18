#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ttk/ttk.h"

ttk_fontinfo* ttk_fonts = 0;

#ifdef IPOD
#define FONTSDIR "/usr/share/fonts"
#else
#define FONTSDIR "fonts"
#endif

int ttk_parse_fonts_list(const char* flf) {
    char buf[128];
    ttk_fontinfo* current = 0;
    int fonts = 0;

    FILE* fp = fopen(flf, "r");
    if (!fp) return 0;

    while (fgets(buf, 128, fp)) {
        if (buf[0] == '#') continue;
        if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
        if (!strlen(buf)) continue;

        if (!strchr(buf, '[') || !strchr(buf, ']') || !strchr(buf, '(') ||
            !strchr(buf, ')') || !strchr(buf, '<') || !strchr(buf, '>')) {
            fprintf(stderr, "Invalid fonts.lst (bad line: |%s|)\n", buf);
            break;
        }

        if (!ttk_fonts) {
            ttk_fonts = current = malloc(sizeof(ttk_fontinfo));
        } else {
            if (!current) {
                current = ttk_fonts;
                while (current->next) current = current->next;
            }
            current->next = malloc(sizeof(ttk_fontinfo));
            current = current->next;
        }

        strncpy(current->file, strchr(buf, '[') + 1, 63);
        strncpy(current->name, strchr(buf, '(') + 1, 63);
        current->file[63] = current->name[63] = 0;
        *strchr(current->file, ']') = 0;
        *strchr(current->name, ')') = 0;
        current->size = atoi(strchr(buf, '<') + 1);
        if (strchr(buf, '{')) {
            char* p = strchr(buf, '{') + 1;
            int sign = 1;
            if (*p == '-') {
                sign = -1;
                p++;
            } else if (*p == '+') {
                p++;
            }
            current->offset = atoi(p) * sign;
        } else {
            current->offset = 0;
        }

        current->refs = 0;
        current->loaded = 0;
        current->next = 0;
        fonts++;
    }
    fclose(fp);
    return fonts;
}

int ttk_parse_fonts_list_dir(const char* dirname) {
    int nfonts = 0;
    int cwdfd = open(".", O_RDONLY);
    if (chdir(dirname) < 0) {
        close(cwdfd);
        return 0;
    }
    DIR* dir = opendir(".");
    struct dirent* d;
    while ((d = readdir(dir)) != 0) {
        if (d->d_name[0] != '.') {
            struct stat st;
            if (stat(d->d_name, &st) >= 0 && S_ISREG(st.st_mode))
                nfonts += ttk_parse_fonts_list(d->d_name);
        }
    }
    closedir(dir);
    fchdir(cwdfd);
    close(cwdfd);
    return nfonts;
}

ttk_fontinfo* ttk_get_fontinfo(const char* name, int size) {
    ttk_fontinfo* current = ttk_fonts;
    ttk_fontinfo* bestmatch = 0;
    int bestmatchsize = -1;

    while (current) {
        if (strcmp(current->name, name) == 0 &&
            (!current->loaded || current->good)) {
            if (current->size == 0) {
                bestmatch = current;
                bestmatchsize = size;
            }
            if ((bestmatchsize < size && current->size > bestmatchsize) ||
                (bestmatchsize > size && current->size < bestmatchsize)) {
                bestmatch = current;
                bestmatchsize = current->size;
            }
        }
        current = current->next;
    }

    if (!bestmatch) {
        current = ttk_fonts;
        while (current) {
            if (!current->loaded || current->good) {
                if (current->size == 0) {
                    bestmatch = current;
                    bestmatchsize = size;
                }
                if ((bestmatchsize < size && current->size > bestmatchsize) ||
                    (bestmatchsize > size && current->size < bestmatchsize)) {
                    bestmatch = current;
                    bestmatchsize = current->size;
                }
            }
            current = current->next;
        }
    }

    if (!bestmatch->loaded) {
        char tmp[256];
        strcpy(tmp, FONTSDIR);
        strcat(tmp, "/");
        strcat(tmp, bestmatch->file);
        bestmatch->good = 1;
        ttk_load_font(bestmatch, tmp, bestmatch->size);
        bestmatch->f->ofs = bestmatch->offset;
        bestmatch->f->fi = bestmatch;
        bestmatch->loaded = 1;
        if (!bestmatch->good) return ttk_get_fontinfo("Any Font", 0);
    }
    bestmatch->refs++;
    return bestmatch;
}

ttk_font ttk_get_font(const char* name, int size) {
    return ttk_get_fontinfo(name, size)->f;
}

void ttk_done_fontinfo(ttk_fontinfo* fi) {
    fi->refs--;
    if (fi->refs <= 0) {
        ttk_unload_font(fi);
        fi->loaded = 0;
    }
}
void ttk_done_font(ttk_font f) { ttk_done_fontinfo(f->fi); }
ttk_fontinfo* ttk_get_fontlist() { return ttk_fonts; }
