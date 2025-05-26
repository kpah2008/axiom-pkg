#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define CONFIG_DIR "/.config/axiom-package"
#define CONFIG_FILE "/.axiomrc"
#define PACKAGE_DIR "/.axpack"
#define MAX_LINE_LENGTH 256
#define MAX_PACKAGES 100
#define MAX_CUSTOM_CMDS 100
#define AXIOMRC_URL "https://raw.githubusercontent.com/Axiom-repo/axiomrc/refs/heads/main/.axiomrc"

typedef struct {
    char name[50];
    char repo[256];
    char repo_type[20];
    char custom_cmds[MAX_CUSTOM_CMDS][256];
    int custom_cmd_count;
    char installed;
} Package;

Package packages[MAX_PACKAGES];
int package_count = 0;

void create_directories() {
    char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        exit(1);
    }

    char config_dir[512];
    snprintf(config_dir, sizeof(config_dir), "%s%s", home, CONFIG_DIR);
    
    struct stat st = {0};
    if (stat(config_dir, &st) == -1) {
        if (mkdir(config_dir, 0755) == -1) {
            perror("Error creating config directory");
            exit(1);
        }
    }

    char package_dir[512];
    snprintf(package_dir, sizeof(package_dir), "%s%s", home, PACKAGE_DIR);
    
    if (stat(package_dir, &st) == -1) {
        if (mkdir(package_dir, 0755) == -1) {
            perror("Error creating package directory");
            exit(1);
        }
    }

    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s%s%s", home, CONFIG_DIR, CONFIG_FILE);
    
    if (stat(config_path, &st) == -1) {
        FILE *file = fopen(config_path, "w");
        if (file == NULL) {
            perror("Error creating config file");
            exit(1);
        }
        fprintf(file, "package:\n\n");
        fclose(file);
    }
}

void update_config_file() {
    char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return;
    }

    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s%s%s", home, CONFIG_DIR, CONFIG_FILE);

    printf("Updating .axiomrc from GitHub...\n");
    
    char download_cmd[512];
    snprintf(download_cmd, sizeof(download_cmd), "curl -s -o %s %s", config_path, AXIOMRC_URL);
    
    if (system(download_cmd) != 0) {
        fprintf(stderr, "Error downloading updated .axiomrc\n");
        return;
    }

    printf("Successfully updated .axiomrc\n");
}

void load_config() {
    char *home = getenv("HOME");
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s%s%s", home, CONFIG_DIR, CONFIG_FILE);
    
    FILE *file = fopen(config_path, "r");
    if (file == NULL) {
        perror("Error opening config file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int in_package_section = 0;
    char current_name[50] = {0};
    char current_repo[256] = {0};
    char current_repo_type[20] = "git";
    char current_cmds[MAX_CUSTOM_CMDS][256] = {0};
    int current_cmd_count = 0;
    int reading_repo = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "package:") == 0) {
            in_package_section = 1;
            continue;
        }

        if (in_package_section) {
            if (line[0] == '[' && strchr(line, ']') != NULL) {
                if (current_name[0] != '\0' && package_count < MAX_PACKAGES) {
                    strcpy(packages[package_count].name, current_name);
                    strcpy(packages[package_count].repo, current_repo);
                    strcpy(packages[package_count].repo_type, current_repo_type);
                    for (int i = 0; i < current_cmd_count; i++) {
                        strcpy(packages[package_count].custom_cmds[i], current_cmds[i]);
                    }
                    packages[package_count].custom_cmd_count = current_cmd_count;
                    
                    char package_path[512];
                    snprintf(package_path, sizeof(package_path), "%s%s/%s", 
                            home, PACKAGE_DIR, current_name);
                    packages[package_count].installed = (access(package_path, F_OK) == 0) ? 1 : 0;
                    
                    package_count++;
                }
                
                char *end = strchr(line, ']');
                strncpy(current_name, line + 1, end - line - 1);
                current_name[end - line - 1] = '\0';
                current_repo[0] = '\0';
                strcpy(current_repo_type, "git");
                memset(current_cmds, 0, sizeof(current_cmds));
                current_cmd_count = 0;
                reading_repo = 1;
            } 
            else if (reading_repo && strlen(line) > 0) {
                if (strncmp(line, "git:", 4) == 0) {
                    strcpy(current_repo_type, "git");
                    strcpy(current_repo, line + 4);
                }
                else if (strncmp(line, "http:", 5) == 0 || strncmp(line, "https:", 6) == 0) {
                    strcpy(current_repo_type, "http");
                    strcpy(current_repo, line);
                }
                else if (strncmp(line, "file:", 5) == 0) {
                    strcpy(current_repo_type, "file");
                    strcpy(current_repo, line + 5);
                }
                else {
                    strcpy(current_repo, line);
                }
                reading_repo = 0;
            }
            else if (strstr(line, "system{") != NULL && strchr(line, '}') != NULL && current_cmd_count < MAX_CUSTOM_CMDS) {
                char *start = strstr(line, "system{") + 7;
                char *end = strchr(line, '}');
                if (start && end) {
                    strncpy(current_cmds[current_cmd_count], start, end - start);
                    current_cmds[current_cmd_count][end - start] = '\0';
                    current_cmd_count++;
                }
            }
        }
    }

    if (current_name[0] != '\0' && package_count < MAX_PACKAGES) {
        strcpy(packages[package_count].name, current_name);
        strcpy(packages[package_count].repo, current_repo);
        strcpy(packages[package_count].repo_type, current_repo_type);
        for (int i = 0; i < current_cmd_count; i++) {
            strcpy(packages[package_count].custom_cmds[i], current_cmds[i]);
        }
        packages[package_count].custom_cmd_count = current_cmd_count;
        
        char package_path[512];
        snprintf(package_path, sizeof(package_path), "%s%s/%s", 
                home, PACKAGE_DIR, current_name);
        packages[package_count].installed = (access(package_path, F_OK) == 0) ? 1 : 0;
        
        package_count++;
    }

    fclose(file);
}

void install_package(const char *package_name) {
    int found = 0;
    Package *pkg = NULL;

    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, package_name) == 0) {
            pkg = &packages[i];
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Error: Package '%s' not found in config\n", package_name);
        exit(1);
    }

    char *home = getenv("HOME");
    char package_path[512];
    snprintf(package_path, sizeof(package_path), "%s%s/%s", home, PACKAGE_DIR, package_name);

    printf("Installing package '%s' from %s (%s)\n", package_name, pkg->repo, pkg->repo_type);

    if (strcmp(pkg->repo_type, "git") == 0) {
        char clone_cmd[512];
        snprintf(clone_cmd, sizeof(clone_cmd), "git clone %s %s", pkg->repo, package_path);
        if (system(clone_cmd) != 0) {
            fprintf(stderr, "Error cloning repository\n");
            exit(1);
        }
    }
    else if (strcmp(pkg->repo_type, "http") == 0 || strcmp(pkg->repo_type, "https") == 0) {
        char download_cmd[512];
        snprintf(download_cmd, sizeof(download_cmd), "mkdir -p %s && cd %s && curl -L %s | tar xz --strip-components=1", 
                package_path, package_path, pkg->repo);
        if (system(download_cmd) != 0) {
            fprintf(stderr, "Error downloading archive\n");
            exit(1);
        }
    }
    else if (strcmp(pkg->repo_type, "file") == 0) {
        char copy_cmd[512];
        snprintf(copy_cmd, sizeof(copy_cmd), "mkdir -p %s && cp -r %s/* %s/", 
                package_path, pkg->repo, package_path);
        if (system(copy_cmd) != 0) {
            fprintf(stderr, "Error copying files\n");
            exit(1);
        }
    }
    else {
        fprintf(stderr, "Error: Unknown repository type '%s'\n", pkg->repo_type);
        exit(1);
    }

    char make_cmd[512];
    snprintf(make_cmd, sizeof(make_cmd), "cd %s && sudo make install", package_path);
    printf("Running 'make install' for package '%s'\n", package_name);
    if (system(make_cmd) != 0) {
        fprintf(stderr, "Error running 'make install'\n");
        exit(1);
    }

    for (int i = 0; i < pkg->custom_cmd_count; i++) {
        printf("Running custom command %d: %s\n", i+1, pkg->custom_cmds[i]);
        char custom_cmd[512];
        snprintf(custom_cmd, sizeof(custom_cmd), "cd %s && %s", package_path, pkg->custom_cmds[i]);
        if (system(custom_cmd) != 0) {
            fprintf(stderr, "Error running custom command\n");
        }
    }

    pkg->installed = 1;
    printf("Successfully installed package '%s'\n", package_name);
}

void remove_package(const char *package_name) {
    char *home = getenv("HOME");
    char package_path[512];
    snprintf(package_path, sizeof(package_path), "%s%s/%s", home, PACKAGE_DIR, package_name);

    if (access(package_path, F_OK) != 0) {
        fprintf(stderr, "Error: Package '%s' is not installed\n", package_name);
        exit(1);
    }

    printf("Removing package '%s'\n", package_name);

    char rm_cmd[512];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", package_path);
    if (system(rm_cmd) != 0) {
        fprintf(stderr, "Error removing package directory\n");
        exit(1);
    }

    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, package_name) == 0) {
            packages[i].installed = 0;
            break;
        }
    }

    printf("Successfully removed package '%s'\n", package_name);
}

void list_all_packages() {
    printf("Available packages:\n");
    for (int i = 0; i < package_count; i++) {
        printf("[%s] %s - %s (%s)\n", 
               packages[i].installed ? "X" : " ", 
               packages[i].name, 
               packages[i].repo,
               packages[i].repo_type);
    }
}

void upgrade_packages() {
    char *home = getenv("HOME");
    
    for (int i = 0; i < package_count; i++) {
        if (packages[i].installed) {
            char package_path[512];
            snprintf(package_path, sizeof(package_path), "%s%s/%s", 
                     home, PACKAGE_DIR, packages[i].name);

            printf("Checking for updates in '%s'...\n", packages[i].name);
            
            if (strcmp(packages[i].repo_type, "git") == 0) {
                char fetch_cmd[512];
                snprintf(fetch_cmd, sizeof(fetch_cmd), "cd %s && git fetch", package_path);
                if (system(fetch_cmd) != 0) {
                    fprintf(stderr, "Error fetching updates for '%s'\n", packages[i].name);
                    continue;
                }

                char check_cmd[512];
                snprintf(check_cmd, sizeof(check_cmd), 
                         "cd %s && git diff --quiet origin/main", package_path);
                int has_updates = system(check_cmd);
                
                if (has_updates != 0) {
                    printf("Updating package '%s'...\n", packages[i].name);
                    
                    char pull_cmd[512];
                    snprintf(pull_cmd, sizeof(pull_cmd), "cd %s && git pull", package_path);
                    if (system(pull_cmd) != 0) {
                        fprintf(stderr, "Error pulling updates for '%s'\n", packages[i].name);
                        continue;
                    }
                } else {
                    printf("Package '%s' is up to date\n", packages[i].name);
                    continue;
                }
            }
            else {
                printf("Package '%s' cannot be automatically updated (non-git repo)\n", packages[i].name);
                continue;
            }
            
            char make_cmd[512];
            snprintf(make_cmd, sizeof(make_cmd), "cd %s && sudo make install", package_path);
            if (system(make_cmd) != 0) {
                fprintf(stderr, "Error reinstalling '%s'\n", packages[i].name);
            } else {
                printf("Successfully updated '%s'\n", packages[i].name);
            }

            for (int j = 0; j < packages[i].custom_cmd_count; j++) {
                printf("Running custom command %d: %s\n", j+1, packages[i].custom_cmds[j]);
                char custom_cmd[512];
                snprintf(custom_cmd, sizeof(custom_cmd), "cd %s && %s", 
                         package_path, packages[i].custom_cmds[j]);
                if (system(custom_cmd) != 0) {
                    fprintf(stderr, "Error running custom command\n");
                }
            }
        }
    }
}

void autoremove() {
    char *home = getenv("HOME");
    char package_dir[512];
    snprintf(package_dir, sizeof(package_dir), "%s%s", home, PACKAGE_DIR);

    printf("Cleaning package directory...\n");
    
    DIR *dir;
    struct dirent *entry;
    
    if ((dir = opendir(package_dir)) == NULL) {
        perror("Error opening package directory");
        exit(1);
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", package_dir, entry->d_name);
        
        char rm_cmd[512];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", path);
        if (system(rm_cmd) != 0) {
            fprintf(stderr, "Error removing %s\n", path);
        }
    }
    
    closedir(dir);
    
    for (int i = 0; i < package_count; i++) {
        packages[i].installed = 0;
    }
    
    printf("Package directory cleaned\n");
}

void print_help() {
    printf("AXiom Package Manager (AxPKG)\n");
    printf("Usage:\n");
    printf("  axpack install <package>  Install a package\n");
    printf("  axpack remove <package>   Remove a package\n");
    printf("  axpack upgrade            Upgrade all installed packages\n");
    printf("  axpack update             Update .axiomrc configuration\n");
    printf("  axpack -all               List all available packages\n");
    printf("  axpack autoremove         Clean package directory\n");
    printf("  axpack help               Show this help message\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    create_directories();
    load_config();

    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name not specified\n");
            return 1;
        }
        install_package(argv[2]);
    } 
    else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name not specified\n");
            return 1;
        }
        remove_package(argv[2]);
    }
    else if (strcmp(argv[1], "upgrade") == 0) {
        upgrade_packages();
    }
    else if (strcmp(argv[1], "update") == 0) {
        update_config_file();
        load_config();
    }
    else if (strcmp(argv[1], "-all") == 0) {
        list_all_packages();
    }
    else if (strcmp(argv[1], "autoremove") == 0) {
        autoremove();
    }
    else if (strcmp(argv[1], "help") == 0) {
        print_help();
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        print_help();
        return 1;
    }

    return 0;
}
