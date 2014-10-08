#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "rtspanalyze.h"

// Local function:
void replace_return_with_null(char*);

/*
 * rtspanalyze -- search for RTSP protocol data from a buffer
 * It is assumed that the buffer contains only one method (not
 * pipelined instructions, where there may be many).
 * Sets the "method" integer field in the structure.
 * Sets those header field pointers that are found.
 * Returns 0 if the method was OK, otherwise nonzero.
 */
int rtspanalyze(char* p_buf, Rtspblock* p_data)
{
  int retval = 0;
  char* strplace;
  char* cz;

  if (!p_buf || !p_data)
    return -1;

  bzero(p_data, sizeof(Rtspblock));

  // Put the method into a number
  if ((strplace = strstr(p_buf, "OPTIONS")) != NULL)
    p_data->method = OPTIONS;
  else if ((strplace = strstr(p_buf, "DESCRIBE")) != NULL)
    p_data->method = DESCRIBE;
  else if ((strplace = strstr(p_buf, "ANNOUNCE")) != NULL)
    p_data->method = ANNOUNCE;
  else if ((strplace = strstr(p_buf, "SETUP")) != NULL)
    p_data->method = SETUP;
  else if ((strplace = strstr(p_buf, "PLAY")) != NULL)
    p_data->method = PLAY;
  else if ((strplace = strstr(p_buf, "PAUSE")) != NULL)
    p_data->method = PAUSE;
  else if ((strplace = strstr(p_buf, "TEARDOWN")) != NULL)
    p_data->method = TEARDOWN;
  else if ((strplace = strstr(p_buf, "GET_PARAMETER")) != NULL)
    p_data->method = GET_PARAMETER;
  else if ((strplace = strstr(p_buf, "SET_PARAMETER")) != NULL)
    p_data->method = SET_PARAMETER;
  else if ((strplace = strstr(p_buf, "REDIRECT")) != NULL)
    p_data->method = REDIRECT;
  else if ((strplace = strstr(p_buf, "RECORD")) != NULL)
    p_data->method = RECORD;
  else
    return 1;

  // Search for headers. Unambiguous cases first:
  p_data->accept = strstr(p_buf,"Accept:");
  p_data->conn = strstr(p_buf,"Connection:");
  p_data->contenc = strstr(p_buf,"Content-Encoding:");
  p_data->contlang = strstr(p_buf,"Content-Language");
  p_data->contlen = strstr(p_buf,"Content-Length:");
  p_data->conttype = strstr(p_buf,"Content-Type:");
  p_data->cseq = strstr(p_buf,"CSeq:");
  p_data->rtpinfo = strstr(p_buf,"RTP-Info:");
  p_data->session = strstr(p_buf,"Session:");
  p_data->transport = strstr(p_buf,"Transport:");
  p_data->unsupp = strstr(p_buf,"Unsupported:");

  // Check for false match (one string is the substring of the other)
  cz = strstr(p_buf,"Proxy-Require:");
  if (cz)
    {
      p_data->proxyrequire = cz;
      cz = strstr(p_buf,"Require:");
      if (cz && cz < p_data->proxyrequire)
	p_data->require = cz;
      else
	p_data->require = strstr(p_data->proxyrequire + 7,"Require: ");
    }
  else
    p_data->require = strstr(p_buf,"Require: ");

  // Adjust the pointer and null-terminate the headers.
  if (p_data->accept)
    {
      replace_return_with_null(p_data->accept);
      p_data->accept += strlen("Accept: ");
    }
  if (p_data->conn)
    {
      replace_return_with_null(p_data->conn);
      p_data->conn += strlen("Connection: ");
    }
  if (p_data->contenc)
    {
      replace_return_with_null(p_data->contenc);
      p_data->contenc += strlen("Content-Encoding: ");
    }
  if (p_data->contlang)
    {
      replace_return_with_null(p_data->contlang);
      p_data->contlang += strlen("Content-Language: ");
    }
  if (p_data->contlen)
    {
      replace_return_with_null(p_data->contlen);
      p_data->contlen += strlen("Content-Length: ");
    }
  if (p_data->conttype)
    {
      replace_return_with_null(p_data->conttype);
      p_data->conttype += strlen("Content-Type: ");
    }
  if (p_data->cseq)
    {
      replace_return_with_null(p_data->cseq);
      p_data->cseq += strlen("CSeq: ");
    }
  if (p_data->proxyrequire)
    {
      replace_return_with_null(p_data->proxyrequire);
      p_data->proxyrequire += strlen("Proxy-Require: ");
    }
  if (p_data->require)
    {
      replace_return_with_null(p_data->require);
      p_data->require += strlen("Require: ");
    }
  if (p_data->rtpinfo)
    {
      replace_return_with_null(p_data->rtpinfo);
      p_data->rtpinfo += strlen("RTP-Info: ");
    }
  if (p_data->session)
    {
      replace_return_with_null(p_data->session);
      p_data->session += strlen("Session: ");
    }
  if (p_data->transport)
    {
      replace_return_with_null(p_data->transport);
      p_data->transport += strlen("Transport: ");
    }
  if (p_data->unsupp)
    {
      replace_return_with_null(p_data->unsupp);
      p_data->unsupp += strlen("Unsupported: ");
    }
  /* Check if required fields were found. Set URL pointer by adding
   * the method string length + 1 (space) to the method pointer value.
   */
  switch (p_data->method)
    {
    case OPTIONS:
      p_data->url = strplace + 8;
      break;
    case DESCRIBE:
      p_data->url = strplace + 9;
      break;
    case ANNOUNCE:
      p_data->url = strplace + 9;
      break;
    case SETUP:
      p_data->url = strplace + 6;
      break;
    case PLAY:
      p_data->url = strplace + 5;
      break;
    case PAUSE:
      p_data->url = strplace + 6;
      break;
    case TEARDOWN:
      p_data->url = strplace + 9;
      break;
    case GET_PARAMETER:
      p_data->url = strplace + 14;
      break;
    case SET_PARAMETER:
      p_data->url = strplace + 14;
      break;
    case REDIRECT:
      p_data->url = strplace + 9;
      break;
    case RECORD:
      p_data->url = strplace + 7;
      break;

    case 0: // never, if handled above already
    default:
      return 1;
    }
  // Null-terminate the URL:
  if ((strplace = strstr(strplace, "RTSP/1.0")) != NULL)
    *(strplace - 1) = '\0';

  return retval;
}

void replace_return_with_null(char *p_str)
{
  char* crplace = strchr(p_str, '\r');
  char* lfplace = strchr(p_str, '\n');
  if (crplace)
    *crplace = '\0';
  if (lfplace)
    *lfplace = '\0';
}
