/* xinput_ps4.c -- the PS4's DualShock 4 through scePad, mapped to the Xbox pad by position.
 *
 * Measured on the console (probe 1): scePadReadState fills buttons, sticks 0..255 with the centre near
 * 122-126, and L2/R2 as 0..255. Only the first pad of the initial user is used (port 0).
 *
 *   Cross -> A   Circle -> B   Square -> X   Triangle -> Y        (positions of the Xbox A B X Y)
 *   L1 -> BLACK  R1 -> WHITE   L2 -> left trigger  R2 -> right trigger
 *   Options -> START   Touch-pad click -> BACK   L3/R3 -> thumb clicks
 *
 * Plain C types only, same shape as xinput_nx.h.
 */

#ifdef __PS4__

#include "xinput_xbox.h"
#include "xinput_nx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <orbis/Pad.h>
#include <orbis/UserService.h>

int ps4_pad_get_handle(void);          /* ps4_orbis.c: opened once on the main thread */

/* The callers run on guest-memory stacks, which the system libraries refuse as buffers (see ps4_orbis.c), so the
 * report and the vibration parameter live in static storage. */
static pthread_mutex_t s_pad_lock = PTHREAD_MUTEX_INITIALIZER;
static OrbisPadData s_pad_data;
static OrbisPadVibeParam s_pad_vibe;

static int pad_handle(void)
{
    return ps4_pad_get_handle();
}

/* 0..255, centre 128, to -32768..32767. */
static int16_t stick16(uint8_t v)
{
    int s = ((int)v - 128) * 256;
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    return (int16_t)s;
}

/* Xbox Y points up, DS4 Y points down. (-1 - v) so -32768 does not overflow. */
static int16_t stick16_inv(uint8_t v)
{
    return (int16_t)(-1 - stick16(v));
}

int xbox_nx_pad_read(unsigned port, uint16_t *digital, uint8_t analog[8], int16_t thumbs[4])
{
    OrbisPadData d;
    uint16_t dig = 0;
    int ok;

    if (port != 0 || pad_handle() < 0)
        return 0;
    pthread_mutex_lock(&s_pad_lock);
    memset(&s_pad_data, 0, sizeof(s_pad_data));
    ok = (scePadReadState(pad_handle(), &s_pad_data) == 0 && s_pad_data.connected);
    d = s_pad_data;
    pthread_mutex_unlock(&s_pad_lock);
    if (!ok)
        return 0;

    if (d.buttons & ORBIS_PAD_BUTTON_UP)       dig |= XBOX_GAMEPAD_DPAD_UP;
    if (d.buttons & ORBIS_PAD_BUTTON_DOWN)     dig |= XBOX_GAMEPAD_DPAD_DOWN;
    if (d.buttons & ORBIS_PAD_BUTTON_LEFT)     dig |= XBOX_GAMEPAD_DPAD_LEFT;
    if (d.buttons & ORBIS_PAD_BUTTON_RIGHT)    dig |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (d.buttons & ORBIS_PAD_BUTTON_OPTIONS)  dig |= XBOX_GAMEPAD_START;
    if (d.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) dig |= XBOX_GAMEPAD_BACK;
    if (d.buttons & ORBIS_PAD_BUTTON_L3)       dig |= XBOX_GAMEPAD_LEFT_THUMB;
    if (d.buttons & ORBIS_PAD_BUTTON_R3)       dig |= XBOX_GAMEPAD_RIGHT_THUMB;
    *digital = dig;

    analog[NX_XBOX_A] = (d.buttons & ORBIS_PAD_BUTTON_CROSS) ? 255 : 0;
    analog[NX_XBOX_B] = (d.buttons & ORBIS_PAD_BUTTON_CIRCLE) ? 255 : 0;
    analog[NX_XBOX_X] = (d.buttons & ORBIS_PAD_BUTTON_SQUARE) ? 255 : 0;
    analog[NX_XBOX_Y] = (d.buttons & ORBIS_PAD_BUTTON_TRIANGLE) ? 255 : 0;
    analog[NX_XBOX_BLACK] = (d.buttons & ORBIS_PAD_BUTTON_L1) ? 255 : 0;
    analog[NX_XBOX_WHITE] = (d.buttons & ORBIS_PAD_BUTTON_R1) ? 255 : 0;
    analog[NX_XBOX_LT] = d.analogButtons.l2;
    analog[NX_XBOX_RT] = d.analogButtons.r2;

    thumbs[0] = stick16(d.leftStick.x);
    thumbs[1] = stick16_inv(d.leftStick.y);
    thumbs[2] = stick16(d.rightStick.x);
    thumbs[3] = stick16_inv(d.rightStick.y);
    return 1;
}

void xbox_nx_pad_rumble(unsigned port, uint16_t left, uint16_t right)
{
    OrbisPadVibeParam v;
    static int off = -1;

    if (off < 0) {
        const char *e = getenv("RECOMP_RUMBLE");
        off = (e && *e == '0') ? 1 : 0;
    }
    if (off || port != 0 || pad_handle() < 0)
        return;
    pthread_mutex_lock(&s_pad_lock);
    s_pad_vibe.lgMotor = (uint8_t)(left >> 8);    /* heavy, low-frequency motor */
    s_pad_vibe.smMotor = (uint8_t)(right >> 8);
    scePadSetVibration(pad_handle(), &s_pad_vibe);
    pthread_mutex_unlock(&s_pad_lock);
}

#endif /* __PS4__ */
