#include "keyboard.h"
#include "isr.h"
#include "io.h"

/*
 * PS/2 Keyboard Driver
 *
 * The keyboard sends scancodes (not ASCII) on IRQ1. Scancode Set 1:
 *   Key press   → "make" code (0x01-0x7F)
 *   Key release → "break" code (make | 0x80)
 *
 * We translate scancodes to ASCII and buffer them in a ring buffer.
 * Modifier keys (shift, caps lock) are tracked for proper case handling.
 */

/* Scancode Set 1 → ASCII (unshifted) */
static const char scancode_map[128] = {
    [0x00] = 0,
    [0x01] = 27,   /* Escape */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', /* Backspace */
    [0x0F] = '\t', /* Tab */
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n', /* Enter */
    [0x1D] = 0,    /* Left Ctrl */
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2A] = 0,    /* Left Shift */
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/',
    [0x36] = 0,    /* Right Shift */
    [0x37] = '*',  /* Keypad * */
    [0x38] = 0,    /* Left Alt */
    [0x39] = ' ',  /* Space */
    [0x3A] = 0,    /* Caps Lock */
};

/* Scancode Set 1 → ASCII (shifted) */
static const char scancode_map_shift[128] = {
    [0x00] = 0,
    [0x01] = 27,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = '\b',
    [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x1D] = 0,
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2A] = 0,
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
    [0x34] = '>', [0x35] = '?',
    [0x36] = 0,
    [0x37] = '*',
    [0x38] = 0,
    [0x39] = ' ',
    [0x3A] = 0,
};

static uint8_t shift_held = 0;
static uint8_t caps_lock  = 0;

/* Ring buffer for keyboard input */
#define KB_BUFFER_SIZE 64
static char kb_buffer[KB_BUFFER_SIZE];
static volatile uint8_t kb_head = 0;
static volatile uint8_t kb_tail = 0;

static void kb_push(char c) {
    uint8_t next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

char keyboard_getchar(void) {
    if (kb_head == kb_tail) return 0;
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

static void keyboard_irq_handler(InterruptFrame *frame) {
    (void)frame;
    uint8_t sc = inb(0x60);    /* Read scancode from PS/2 data port */

    uint8_t released = sc & 0x80;
    uint8_t key      = sc & 0x7F;

    /* Track shift keys */
    if (key == 0x2A || key == 0x36) {
        shift_held = !released;
        return;
    }

    /* Caps Lock toggles on press */
    if (key == 0x3A && !released) {
        caps_lock = !caps_lock;
        return;
    }

    /* Ignore key releases for everything else */
    if (released) return;

    /* Look up ASCII character */
    int use_shift = shift_held;

    /* Caps Lock only affects letters (a-z) */
    char base = scancode_map[key];
    if (caps_lock && base >= 'a' && base <= 'z')
        use_shift = !use_shift;

    char c = use_shift ? scancode_map_shift[key] : scancode_map[key];
    if (c)
        kb_push(c);
}

void keyboard_init(void) {
    isr_register_irq(1, keyboard_irq_handler);
}
