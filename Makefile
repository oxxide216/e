.RECIPEPREFIX = >

CC = cc
override CFLAGS += -Wall -Wextra -Ilibs -Ilibs/lexgen/include -ggdb
override LDFLAGS +=
BUILD_DIR = build

EC_C_SRC = $(wildcard src/ec-c/*.c)

EC_C_OBJ = $(patsubst src/ec-c/%.c,$(BUILD_DIR)/ec-c/%.o,$(EC_C_SRC))

all: ec-c

ec-c: $(EC_C_OBJ)
> $(CC) -o ec-c $(EC_C_OBJ) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c src/ec-c/grammar.h
> mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/libs/%.o: libs/%.c
> mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -c -o $@ $<

src/compiler/grammar.h: libs/lexgen/lexgen grammar.lg
> libs/lexgen/lexgen src/compiler/grammar.h grammar.lg

libs/lexgen/lexgen:
> cd libs/lexgen && ./build.sh

clean:
> rm -rf $(BUILD_DIR) ec-c
