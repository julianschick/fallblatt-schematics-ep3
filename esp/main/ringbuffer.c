
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char buffer[255];
int end = 0;


void command(char* cmd, char* arg1, char* arg2) {
	if (strcmp(cmd, "FLAP") == 0) {
		char* ptr;
		int flap = strtol(arg1, &ptr, 10);

		if (ptr != arg1 && flap >= 0 && flap < 40) {
			printf("drive to flap %d\n", flap);
		}
	}
}

int main() {

	while(1) {
		char c = getchar();

		if (end != 256 && c != 10) {
			buffer[end] = c;
			printf("input added %d\n", c);
			end++;
		}

		int first_newline = -1;

		for (int i = 0; i < end; i++) {
			if (buffer[i] == 'E') {
				first_newline = i;
				break;
			}
		}

		if (first_newline != -1) {
			buffer[first_newline] = 0x00;

			char* str = (char*)calloc(first_newline + 1, sizeof(char));

			strcpy(str, buffer);
			char* delimiter = " ";

			char* ptr = strtok(str, delimiter);
			char cmd[256] = "";
			char arg1[256] = "";
			char arg2[256] = "";
			int i = 0;
			while(ptr != NULL) {
				
				if (i == 0) {
					strcpy(cmd, ptr);
				} else if (i == 1) {
					strcpy(arg1, ptr);
				} else if (i == 2) {
					strcpy(arg2, ptr);
				}
				// naechsten Abschnitt erstellen
 				ptr = strtok(NULL, delimiter);
 				i++;
			}

			printf("command: %s\n", cmd);
			printf("arg1: %s\n", arg1);
			printf("arg2: %s\n", arg2);

			command(cmd, arg1, arg2);

			free(str);

			if (first_newline != 255) {

				for (int i = first_newline + 1; i < 256; i++) {
					buffer[i - (first_newline + 1)] = buffer[i];
				}
				end -= first_newline + 1;
			} else {
				end = 0;
			}
		}
	}

}