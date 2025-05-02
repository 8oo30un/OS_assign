#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 1024

// 문자열 길이
int my_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

// strstr 구현 (포인터 반환)
char *my_strstr(const char *haystack, const char *needle) {
    int hlen = my_strlen(haystack);
    int nlen = my_strlen(needle);

    if (nlen == 0) return (char *)haystack;

    for (int i = 0; i <= hlen - nlen; i++) {
        int found = 1;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] != needle[j]) {
                found = 0;
                break;
            }
        }
        if (found) return (char *)&haystack[i];
    }
    return NULL;
}

// ANSI 빨간색 강조
void highlight_word(const char *line, const char *word) {
    const char *pos = line;
    int word_len = my_strlen(word);
    while (*pos) {
        char *found = my_strstr(pos, word);
        if (!found) {
            printf("%s", pos);
            break;
        }
        fwrite(pos, 1, found - pos, stdout);
        printf("\033[1;31m%.*s\033[0m", word_len, found);
        pos = found + word_len;
    }
}

int main(int argc, char *argv[]) {
    char *command = NULL;
    char *search = NULL;
    int use_range = 0;
    int start_line = 0, end_line = 0;

    // 🔧 기능 추가: --line-range start end 옵션 지원
    if (argc == 6 && strcmp(argv[1], "--line-range") == 0) {
        use_range = 1;
        start_line = atoi(argv[2]);
        end_line = atoi(argv[3]);
        command = argv[4];
        search = argv[5];
    } else if (argc == 3) {
        command = argv[1];
        search = argv[2];
    } else {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s \"<command>\" <search_word>\n", argv[0]);
        fprintf(stderr, "  %s --line-range <start> <end> \"<command>\" <search_word>\n", argv[0]);
        exit(1);
    }

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    else if (pid == 0) {
        // 자식
        close(fd[0]); // 읽기 닫음
        dup2(fd[1], STDOUT_FILENO); // stdout을 파이프로 연결
        close(fd[1]);

        // command split
        char *args[64];
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }

    else {
        // 부모
        close(fd[1]); // 쓰기 닫음
        FILE *fp = fdopen(fd[0], "r");
        if (!fp) {
            perror("fdopen");
            exit(1);
        }

        char line[MAX_LINE];
        int lineno = 1;
        while (fgets(line, sizeof(line), fp)) {
            // ✅ 추가된 기능: 줄 범위 내일 경우만 출력
            if (!use_range || (lineno >= start_line && lineno <= end_line)) {
                if (my_strstr(line, search)) {
                    printf("%d: ", lineno);
                    highlight_word(line, search);
                }
            }
            lineno++;
        }

        fclose(fp);
        wait(NULL); // 자식 종료 대기
    }

    return 0;
}