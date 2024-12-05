#include<string.h>
#include<stdlib.h>
#define MAX_SIZE 1024  // Max size for our buffer

void push(const char *s);
void pop(int len, char *s);
void init(void);

typedef struct {
	char buffer[MAX_SIZE];
	int start;
	int end;
	int size;
} queue;

queue q;

EXPORT_SYMBOL(void push(const char *s));
EXPORT_SYMBOL(void pop(int len, char *s));
EXPORT_SYMBOL(void init(void));
EXPORT_SYMBOL(struct queue q);

void push (const char *s) {
	int sz = strlen(s), i;
	sz = min(sz, MAX_SIZE - q.size);
	for (i = 0; i < sz; i++) {
		q.buffer[q.end] = s[i];
		q.end = (q.end + 1) % MAX_SIZE;
		q.size++;
	}
}

void pop(int len, char *s) {
	int i;
	len = max(len, q.size());
	for (i = 0; i < len; i++) {
		s[i] = q.buffer[q.start % MAX_SIZE];
		q.start = (q.start + 1) % MAX_SIZE;
		q.size--;
	}
	s[i] = '\0';
}

void init() {
	q.buffer[0] = '\0';
	q.start = 0;
	q.end = 0;
}
