/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2011 Ingo Ruhnke <grumbel@gmx.de>
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "virtualkeyboard/keyboard_controller.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdexcept>

#include "log.hpp"
#include "virtualkeyboard/virtual_keyboard.hpp"

KeyboardController::KeyboardController(VirtualKeyboard& keyboard, const std::string& device) :
  m_keyboard(keyboard),
  m_device(device),
  m_fd(-1),
  m_io_channel(0),
  m_timeout_source(-1),
  m_stick_x(0),
  m_stick_y(0)
{
  m_fd = open(m_device.c_str(), O_RDONLY | O_NONBLOCK);

  if (m_fd == -1)
  {
    throw std::runtime_error(m_device + ": " + std::string(strerror(errno)));
  }

  m_io_channel = g_io_channel_unix_new(m_fd);

  // set encoding to binary
  GError* error = NULL;
  if (g_io_channel_set_encoding(m_io_channel, NULL, &error) != G_IO_STATUS_NORMAL)
  {
    log_error(error->message);
    g_error_free(error);
  }
  g_io_channel_set_buffered(m_io_channel, false);
    
  guint source_id;
  source_id = g_io_add_watch(m_io_channel, 
                             static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
                             &KeyboardController::on_read_data_wrap, this);

  // FIXME: Could limit calling this to when the stick actually moved
  m_timeout_source = g_timeout_add(25, &KeyboardController::on_timeout_wrap, this);
}

KeyboardController::~KeyboardController()
{
  g_io_channel_unref(m_io_channel);
  close(m_fd);

  if (m_timeout_source > 0)
    g_source_remove(m_timeout_source);
}

void
KeyboardController::parse(const struct input_event& ev)
{
  //log_tmp(ev.type << " " << ev.code << " " << ev.value);
  if (ev.type == EV_ABS)
  {
    if (ev.code == ABS_HAT0X)
    {
      if (ev.value == -1)
      {
        m_keyboard.cursor_left();
      }
      else if (ev.value == 1)
      {
        m_keyboard.cursor_right();
      }
    }
    else if (ev.code == ABS_HAT0Y)
    {
      if (ev.value == -1)
      {
        m_keyboard.cursor_up();
      }
      else if (ev.value == 1)
      {
        m_keyboard.cursor_down();
      }
    }
    else if (ev.code == ABS_RX)
    {
      if (abs(ev.value) > 8000)
      {
        //m_keyboard.move(ev.value / 4, 0);
        m_stick_x = ev.value / 32768.0f;
      }
      else
      {
        m_stick_x = 0.0f;
      }
    }
    else if (ev.code == ABS_RY)
    {
      if (abs(ev.value) > 8000)
      {
        m_stick_y = ev.value / -32768.0f;
        //m_keyboard.move(0, ev.value / 40);
      }
      else
      {
        m_stick_y = 0.0f;
      }
    }
  }
  else if (ev.type == EV_KEY)
  {
    switch(ev.code)
    {
      case kSendButton:
        m_keyboard.send_key(ev.value);
        break;

      case kHoldButton:
        //m_hold_key.send_key();
        break;

      case kCancelHoldButton:
        //m_keyboard.cancel_holds();
        break;

      case kShiftButton:
        //m_keyboard.send_key(KEY_LEFTSHIFT, ev.value);
        break;

      case kCtrlButton:
        //m_keyboard.send_key(KEY_LEFTCTRL, ev.value);
        break;

      case kBackspaceButton:
        //m_keyboard.send_key(KEY_BACKSPACE, ev.value);
        break;

      case kHideButton:
        if (ev.value)
        {
          m_keyboard.show();
        }
        else
        {
          m_keyboard.hide();
        }
        break;
    }
  }
}

gboolean
KeyboardController::on_read_data(GIOChannel* source, GIOCondition condition)
{
  // read data
  struct input_event ev[128];
  int rd = 0;
  while((rd = ::read(m_fd, ev, sizeof(struct input_event) * 128)) > 0)
  {
    for (size_t i = 0; i < rd / sizeof(struct input_event); ++i)
    {
      parse(ev[i]);
    }
  }
  
  return TRUE;
}

bool
KeyboardController::on_timeout()
{
  int x;
  int y;

  m_keyboard.get_position(&x, &y);
  x += m_stick_x * 40.0f;
  y += m_stick_y * 40.0f;
  m_keyboard.move(x, y);

  return true;
}

/* EOF */
