#pragma once

String format_duration(time_t seconds)
{
  auto days = seconds / (60 * 60 * 24);
  auto tm = gmtime(&seconds);
  String duration = days > 0 ? String(days) + " days, " : "";
  char time_buff[9];
  strftime(time_buff, 9, "%H:%M:%S", tm);
  return duration + time_buff;
}