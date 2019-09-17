// Brexit clock
// Copyright (c) 2019 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)
const char TAG[] = "Brexit";

#include "revk.h"
#include <driver/i2c.h>
#include <math.h>

#define ACK_CHECK_EN 0x1        /*!< I2C master will check ack from slave */
#define ACK_CHECK_DIS 0x0       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0             /*!< I2C ack value */
#define NACK_VAL 0x1            /*!< I2C nack value */
#define	MAX_OWB	8
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)

#define settings	\
	s8(oledsda,5)	\
	s8(oledscl,18)	\
	s8(oledaddress,0x3D)	\
	u8(oledcontrast,127)	\
	b(oledflip)	\
	b(f)	\
	s(deadline,CONFIG_BREXIT_DEADLINE)	\

#define u32(n,d)	uint32_t n;
#define s8(n,d)	int8_t n;
#define u8(n,d)	int8_t n;
#define b(n) uint8_t n;
#define s(n,d) char * n;
settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
static SemaphoreHandle_t oledi2c_mutex = NULL;
static SemaphoreHandle_t oled_mutex = NULL;
static int8_t oledport = -1;

static volatile uint8_t oled_update = 0;
static volatile uint8_t oled_contrast = 0;
static volatile uint8_t oled_changed = 1;

const char *
app_command (const char *tag, unsigned int len, const unsigned char *value)
{
   if (!strcmp (tag, "contrast"))
   {
      xSemaphoreTake (oled_mutex, portMAX_DELAY);
      oled_contrast = atoi ((char *) value);
      if (oled_update)
         oled_update = 1;
      xSemaphoreGive (oled_mutex);
      return "";                // OK
   }
   return NULL;
}

#define	OLEDW	128
#define	OLEDH	128
#define	OLEDB	4
static uint8_t oled[OLEDW * OLEDH * OLEDB / 8];

static inline void
oledbit (int x, int y, uint8_t v)
{
   uint8_t m = (1 << OLEDB) - 1;
   if (x < 0 || x >= OLEDW)
      return;
   if (y < 0 || y >= OLEDH)
      return;
   if (v > m)
      v = m;
   int s = ((8 / OLEDB) - 1 - (x % (8 / OLEDB))) * OLEDB;
   int o = y * OLEDW * OLEDB / 8 + x * OLEDB / 8;
   uint8_t new = ((oled[o] & ~(m << s)) | (v << s));
   if (oled[o] == new)
      return;
   oled[o] = new;
   oled_changed = 1;
}

static inline int
oledcopy (int x, int y, const uint8_t * src, int dx)
{                               // Copy pixels
   x -= x % (8 / OLEDB);        // Align to byte
   dx -= dx % (8 / OLEDB);      // Align to byte
   if (y >= 0 && y < OLEDH && x + dx >= 0 && x < OLEDW)
   {                            // Fits
      int pix = dx;
      if (x < 0)
      {                         // Truncate left
         pix += x;
         x = 0;
      }
      if (x + pix > OLEDW)
         pix = OLEDW - x;       // Truncate right
      uint8_t *dst = oled + y * OLEDW * OLEDB / 8 + x * OLEDB / 8;
      if (memcmp (dst, src, pix * OLEDB / 8))
      {                         // Changed
         memcpy (dst, src, pix * OLEDB / 8);
         oled_changed = 1;
      }
   }
   return dx * OLEDB / 8;       // Bytes (would be) copied
}

#include CONFIG_BREXIT_LOGO
#include "font1.h"
#include "font2.h"
#include "font3.h"
#include "font4.h"
#include "font5.h"
const uint8_t *const fonts[] = { font1, font2, font3, font4, font5 };

int
text (int8_t size, int x, int y, char *t)
{                               // Size (negative for descenders)
   int z = 7;
   if (size < 0)
   {
      size = -size;
      z = 9;
   }
   if (!size)
      size = 1;
   else if (size > sizeof (fonts) / sizeof (*fonts))
      size = sizeof (fonts) / sizeof (*fonts);
   int w = size * 6;
   int h = size * 9;
   y -= size * 2;               // Baseline
   while (*t)
   {
      int c = *t++;
      if (c >= 0x7F)
         continue;
      const uint8_t *base = fonts[size - 1] + (c - ' ') * h * w * OLEDB / 8;
      int ww = w;
      if (c < ' ')
      {                         // Sub space
         ww = size * c;
         c = ' ';
      }
      if (c == '.' || c == ':')
      {
         ww = size * 2;
         base += size * 2 * OLEDB / 8;
      }                         // Special case for .
      c -= ' ';
      for (int dy = 0; dy < size * z; dy++)
      {
         oledcopy (x, y + h - 1 - dy, base, ww);
         base += w * OLEDB / 8;
      }
      x += ww;
   }
   return x;
}

static void
oled_task (void *p)
{
   int try = 10;
   esp_err_t e;
   while (try--)
   {
      xSemaphoreTake (oledi2c_mutex, portMAX_DELAY);
      oled_changed = 0;
      i2c_cmd_handle_t t = i2c_cmd_link_create ();
      i2c_master_start (t);
      i2c_master_write_byte (t, (oledaddress << 1) | I2C_MASTER_WRITE, true);
      i2c_master_write_byte (t, 0x00, true);    // Cmds
      i2c_master_write_byte (t, 0xA5, true);    // White
      i2c_master_write_byte (t, 0xAF, true);    // On
      i2c_master_write_byte (t, 0xA0, true);    // Remap
      i2c_master_write_byte (t, oledflip ? 0x52 : 0x41, true);  // Match display
      i2c_master_stop (t);
      e = i2c_master_cmd_begin (oledport, t, 10 / portTICK_PERIOD_MS);
      i2c_cmd_link_delete (t);
      xSemaphoreGive (oledi2c_mutex);
      if (!e)
         break;
      sleep (1);
   }
   if (e)
   {
      revk_error ("OLED", "Configuration failed %s", esp_err_to_name (e));
      vTaskDelete (NULL);
      return;
   }

   memset (oled, 0x00, sizeof (oled));  // Blank
   {
      int w = sizeof (logo[0]) * 8 / OLEDB;
      int h = sizeof (logo) / sizeof (*logo);
      for (int dy = 0; dy < h; dy++)
         oledcopy (OLEDW - w, 10 + h - dy, logo[dy], w);
      text (1, 0, 0, CONFIG_BREXIT_TAG);
   }

   while (1)
   {                            // Update
      if (!oled_changed)
      {
         usleep (100000);
         continue;
      }
      xSemaphoreTake (oledi2c_mutex, portMAX_DELAY);
      xSemaphoreTake (oled_mutex, portMAX_DELAY);
      oled_changed = 0;
      i2c_cmd_handle_t t;
      e = 0;
      if (oled_update < 2)
      {                         // Set up
         t = i2c_cmd_link_create ();
         i2c_master_start (t);
         i2c_master_write_byte (t, (oledaddress << 1) | I2C_MASTER_WRITE, true);
         i2c_master_write_byte (t, 0x00, true); // Cmds
         if (oled_update)
            i2c_master_write_byte (t, 0xA4, true);      // Normal mode
         i2c_master_write_byte (t, 0x81, true); // Contrast
         i2c_master_write_byte (t, oled_contrast, true);        // Contrast
         i2c_master_write_byte (t, 0x15, true); // Col
         i2c_master_write_byte (t, 0x00, true); // 0
         i2c_master_write_byte (t, 0x7F, true); // 127
         i2c_master_write_byte (t, 0x75, true); // Row
         i2c_master_write_byte (t, 0x00, true); // 0
         i2c_master_write_byte (t, 0x7F, true); // 127
         i2c_master_stop (t);
         e = i2c_master_cmd_begin (oledport, t, 100 / portTICK_PERIOD_MS);
         i2c_cmd_link_delete (t);
      }
      if (!e)
      {                         // data
         t = i2c_cmd_link_create ();
         i2c_master_start (t);
         i2c_master_write_byte (t, (oledaddress << 1) | I2C_MASTER_WRITE, true);
         i2c_master_write_byte (t, 0x40, true); // Data
         i2c_master_write (t, oled, sizeof (oled), true);       // Buffer
         i2c_master_stop (t);
         e = i2c_master_cmd_begin (oledport, t, 100 / portTICK_PERIOD_MS);
         i2c_cmd_link_delete (t);
      }
      if (e)
         revk_error ("OLED", "Data failed %s", esp_err_to_name (e));
      if (!oled_update || e)
      {
         oled_update = 1;       // Resend data
         oled_changed = 1;
      } else
         oled_update = 2;       // All OK
      xSemaphoreGive (oled_mutex);
      xSemaphoreGive (oledi2c_mutex);
   }
}

void
app_main ()
{
   revk_init (&app_command);
#define b(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s(n,d) revk_register(#n,0,0,&n,d,0);
   settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
      oled_mutex = xSemaphoreCreateMutex ();    // Shared text access
   oled_contrast = oledcontrast;        // Initial contrast
   if (oledsda >= 0 && oledscl >= 0)
   {                            // Separate OLED port
      oledport = 1;
      if (i2c_driver_install (oledport, I2C_MODE_MASTER, 0, 0, 0))
      {
         revk_error ("OLED", "I2C config fail");
         oledport = -1;
      } else
      {
         i2c_config_t config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = oledsda,
            .scl_io_num = oledscl,
            .sda_pullup_en = true,
            .scl_pullup_en = true,
            .master.clk_speed = 100000,
         };
         if (i2c_param_config (oledport, &config))
         {
            i2c_driver_delete (oledport);
            revk_error ("OLED", "I2C config fail");
            oledport = -1;
         } else
            i2c_set_timeout (oledport, 160000); // 2ms? allow for clock stretching
      }
      oledi2c_mutex = xSemaphoreCreateMutex ();
   }
   if (oledport >= 0)
      revk_task ("OLED", oled_task, NULL);
   // Main task...
   while (1)
   {
      xSemaphoreTake (oled_mutex, portMAX_DELAY);
      char s[30];
      static time_t showtime = 0;
      time_t now = time (0);
      if (now != showtime)
      {
         showtime = now;
         struct tm nowt;
         localtime_r (&showtime, &nowt);
         if (nowt.tm_year > 100)
         {
            strftime (s, sizeof (s), "%F\004%T %Z", &nowt);
            text (1, 0, 0, s);
            int X,
              Y = OLEDH - 1;
            if (deadline)
            {
               // Work out the time left
               int y = 0,
                  m = 0,
                  d = 0,
                  H = 0,
                  M = 0,
                  S = 0;
               sscanf (deadline, "%d-%d-%d %d:%d:%d", &y, &m, &d, &H, &M, &S);
               if (!m)
                  m = 1;
               if (!d)
                  d = 1;
               if (!y)
               {                // Regular date
                  y = nowt.tm_year + 1900;
                  if (nowt.tm_mon * 2678400 + nowt.tm_mday * 86400 + nowt.tm_hour * 3600 + nowt.tm_min * 60 + nowt.tm_sec >
                      (m - 1) * 2678400 + d * 86400 + H * 3600 + M * 60 + S)
                     y++;
               }
               // Somewhat convoluted to allow for clock changes
               int days = 0;
               {                // Work out days (using H:M:S non DST)
                struct tm target = { tm_year: y - 1900, tm_mon: m - 1, tm_mday:d };
                struct tm today = { tm_year: nowt.tm_year, tm_mon: nowt.tm_mon, tm_mday:nowt.tm_mday };
                  days = (mktime (&target) - mktime (&today)) / 86400;
                  if (nowt.tm_hour * 3600 + nowt.tm_min * 60 + nowt.tm_sec > H * 3600 + M * 60 + S)
                     days--;    // Passed current time
               }
             struct tm deadt = { tm_year: y - 1900, tm_mon: m - 1, tm_mday: d, tm_hour: H, tm_min: M, tm_sec: S, tm_isdst:-1 };
               nowt.tm_mday += days;    // Current local time, this many days ahead
               nowt.tm_isdst = -1;
               int seconds = mktime (&deadt) - mktime (&nowt);
               if (days < 0)
                  days = seconds = 0;   // Deadline reached
               sprintf (s, "%4d", days);
               Y -= 5 * 7;
               X = text (5, 0, Y, s);
               text (1, X, Y + 3 * 8, "D");
               text (1, X, Y + 2 * 8, "A");
               text (1, X, Y + 1 * 8, "Y");
               text (1, X, Y + 0 * 8, days == 1 ? " " : "S");
               Y -= 4 * 7 + 3;
               sprintf (s, "%02d", seconds / 3600);
               X = text (4, 0, Y, s);
               sprintf (s, ":%02d", seconds / 60 % 60);
               X = text (3, X, Y, s);
               sprintf (s, ":%02d", seconds % 60);
               X = text (2, X, Y, s);
            }
         }
      }
      xSemaphoreGive (oled_mutex);
      // Next second
      usleep (1000000LL - (esp_timer_get_time () % 1000000LL));
   }
}
