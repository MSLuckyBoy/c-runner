#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ensure_clang_format_config() {
    const char *filename = ".clang-format";
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return;
    }

    printf("[INFO] Файл %s не найден. Создаю с настройками по умолчанию...\n", filename);
    file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "[ОШИБКА] Не удалось создать %s\n", filename);
        exit(1);
    }

    const char *config =
        "---\n"
        "BasedOnStyle: Google\n"
        "IndentWidth: 4\n"
        "ColumnLimit: 110\n";

    fputs(config, file);
    fclose(file);
    printf("[INFO] Файл %s создан.\n", filename);
}

int run_command(const char *desc, const char *cmd) {
    printf("\n==> %s:\n$ %s\n", desc, cmd);
    int ret = system(cmd);
    if (ret != 0) {
        int exit_code = WEXITSTATUS(ret);
        fprintf(stderr, "[ОШИБКА] Команда завершилась с кодом %d: %s\n", exit_code, cmd);
    }
    return ret;
}

int check_valgrind_log(const char *logfile) {
    FILE *log = fopen(logfile, "r");
    if (!log) {
        fprintf(stderr, "[ОШИБКА] Не удалось открыть %s\n", logfile);
        return -1;
    }

    char line[512];
    int leaks_found = 0;

    while (fgets(line, sizeof(line), log)) {
        if (strstr(line, "definitely lost") && !strstr(line, "definitely lost: 0 bytes")) {
            leaks_found = 1;
            break;
        }
    }

    fclose(log);

    if (leaks_found) {
        fprintf(stderr, "[ОШИБКА] Обнаружены утечки памяти!\n");
        return 1;
    } else {
        printf("[INFO] Утечек памяти не обнаружено.\n");
        return 0;
    }
}

void print_help(const char *program_name) {
    printf("Использование: %s <файл.c> [--keep-log]\n", program_name);
    printf("Пример: %s my_code.c --keep-log\n\n", program_name);
    printf("Действия:\n");
    printf("  - Форматирует исходный код через clang-format\n");
    printf("  - Компилирует с помощью gcc (-g)\n");
    printf("  - Запускает valgrind для поиска утечек памяти\n");
    printf("  - Проверяет отчет на наличие утечек\n");
    printf("\nПараметры:\n");
    printf("  --keep-log   не удалять файл valgrind.log после проверки\n");
    printf("  -h, --help   показать эту справку\n");
    printf("\nЕсли файл .clang-format не найден — создается с настройками по умолчанию.\n");
}

int main(int argc, char *argv[]) {
    char *src_file = NULL;
    int keep_log = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--keep-log") == 0) {
            keep_log = 1;
        } else if (!src_file && strstr(argv[i], ".c")) {
            src_file = argv[i];
        }
    }

    if (!src_file) {
        fprintf(stderr, "[ОШИБКА] Укажите C-файл с расширением .c\n");
        return 1;
    }

    char exe_file[] = "./program";
    char command[512];
    const char *valgrind_log = "valgrind.log";

    ensure_clang_format_config();

    snprintf(command, sizeof(command), "clang-format -i %s", src_file);
    if (run_command("Форматирование исходного файла", command) != 0) return 1;

    snprintf(command, sizeof(command), "gcc %s -o %s -g 2>&1", src_file, exe_file);
    if (run_command("Компиляция", command) != 0) return 1;

    snprintf(command, sizeof(command), "valgrind --leak-check=full --track-origins=yes %s 2>&1 | tee %s",
             exe_file, valgrind_log);
    run_command("Проверка через valgrind", command);

    int result = check_valgrind_log(valgrind_log);

    if (!keep_log) {
        remove(valgrind_log);
    }

    return result;
}
