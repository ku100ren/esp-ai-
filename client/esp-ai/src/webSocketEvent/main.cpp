/**
 * Copyright (c) 2024 小明IO
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Commercial use of this software requires prior written authorization from the Licensor.
 * 请注意：将 ESP-AI 代码用于商业用途需要事先获得许可方的授权。
 * 删除与修改版权属于侵权行为，请尊重作者版权，避免产生不必要的纠纷。
 *
 * @author 小明IO
 * @email  1746809408@qq.com
 * @github https://github.com/wangzongming/esp-ai
 * @websit https://espai.fun
 */
#include "main.h"

void ESP_AI::webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        if (ws_connected)
        {
            ws_connected = false;
            can_voice = "1";
            start_ed = "0";
            session_id = "";
            digitalWrite(LED_BUILTIN, LOW);
            Serial.println("[Info] -> WebSocket Disconnected\n");

            // 设备状态回调
            if (onNetStatusCb != nullptr)
            {
                net_status = "2";
                onNetStatusCb("2");
            }
        }
        break;
    case WStype_CONNECTED:
    {
        ws_connected = true;
        can_voice = "1";
        start_ed = "0";
        session_id = "";
        Serial.println("\n[Info] -> WebSocket Connected");

        JSONVar data_1;
        data_1["type"] = "play_audio_ws_conntceed";
        String sendData = JSON.stringify(data_1);
        webSocket.sendTXT(sendData);

        // 设备状态回调
        if (onNetStatusCb != nullptr)
        {
            net_status = "3";
            onNetStatusCb("3");
        }
        break;
    }
    case WStype_TEXT:
        if (strcmp((char *)payload, "start_voice") == 0)
        {
            can_voice = "1";
            DEBUG_PRINTLN(debug, ("[Info] -> 继续采集音频"));
        }
        else if (strcmp((char *)payload, "pause_voice") == 0)
        {
            can_voice = "0";
            DEBUG_PRINTLN(debug, ("[Info] -> 暂停采集音频"));
        }
        else if (strcmp((char *)payload, "session_end") == 0)
        {
            start_ed = "0";
            can_voice = "1";
            session_id = "";
            digitalWrite(LED_BUILTIN, LOW);
            DEBUG_PRINTLN(debug, ("\n[Info] -> 会话结束\n"));
        }
        else
        {
            JSONVar parseRes = JSON.parse((char *)payload);
            if (JSON.typeof(parseRes) == "undefined")
            {
                return;
            }
            if (parseRes.hasOwnProperty("type"))
            {
                String type = (const char *)parseRes["type"];
                String command_id = "";
                String data = "";
                if (parseRes.hasOwnProperty("command_id"))
                {
                    command_id = (const char *)parseRes["command_id"];
                }
                if (parseRes.hasOwnProperty("data"))
                {
                    data = (const char *)parseRes["data"];
                }

                if (type == "stc_time")
                {
                    String stc_time = parseRes["stc_time"];
                    JSONVar data_delayed;
                    data_delayed["type"] = "cts_time";
                    data_delayed["stc_time"] = stc_time;
                    String sendData = JSON.stringify(data_delayed);
                    webSocket.sendTXT(sendData);
                }

                if (type == "net_delay")
                {
                    long net_delay = parseRes["net_delay"];
                    DEBUG_PRINTLN(debug, ("\n======================================="));
                    DEBUG_PRINTLN(debug, "网络延时：" + String(net_delay) + "ms");
                    DEBUG_PRINTLN(debug, ("=======================================\n"));
                }

                // user command
                if (type == "instruct")
                {
                    DEBUG_PRINTLN(debug, "[instruct] -> 客户端收到用户指令：" + command_id + " --- " + data);
                    if (onEventCb != nullptr)
                    {
                        onEventCb(command_id, data);
                    }
                }

                // tts task log
                if (type == "play_audio")
                {
                    tts_task_id = (const char *)parseRes["tts_task_id"];
                    String now_session_id = (const char *)parseRes["session_id"];
                    DEBUG_PRINTLN(debug, "\n[TTS] -> TTS 任务：" + tts_task_id + " 所属会话：" + now_session_id);
                }

                if (type == "session_start")
                {
                    String now_session_id = (const char *)parseRes["session_id"];
                    DEBUG_PRINTLN(debug, "\n[Info] -> 会话开始：" + now_session_id);
                    session_id = now_session_id;
                }
                if (type == "tts_send_end")
                {
                    String end_tts_task_id = (const char *)parseRes["tts_task_id"];
                    DEBUG_PRINTLN(debug, "\n[TTS] -> 客户端收到 TTS 任务结束：" + end_tts_task_id);

                    // 这里其他给情况预留服务回调
                    is_send_server_audio_over = "1";
                    can_voice = "1";
                    digitalWrite(LED_BUILTIN, LOW);
                    JSONVar data;
                    data["type"] = "client_out_audio_over";
                    data["tts_task_id"] = end_tts_task_id;
                    data["session_id"] = session_id;
                    String sendData = JSON.stringify(data);
                    DEBUG_PRINTLN(debug, ("\n[TTS] -> 发送播放结束标识到服务端"));
                    webSocket.sendTXT(sendData);
                    tts_task_id = "";
                }

                if (type == "auth_fail")
                {
                    String message = (const char *)parseRes["message"];
                    Serial.println("[Error] -> 连接服务失败，鉴权失败：" + message);
                    Serial.println(F("[Error] -> 请检测服务器配置中是否配置了鉴权参数。"));
                    if (onErrorCb != nullptr)
                    {
                        onErrorCb("002", "auth", message);
                    }
                }
                if (type == "error")
                {
                    String at_pos = (const char *)parseRes["at"];
                    String message = (const char *)parseRes["message"];
                    String code = (const char *)parseRes["code"];
                    Serial.println("[Error] -> 服务错误：" + at_pos + " " + code + " " + message);
                    if (onErrorCb != nullptr)
                    {
                        onErrorCb(code, at_pos, message);
                    }
                }
            }
        }

        Serial.printf("::-> Received Text: %s\n", payload);
        break;
    case WStype_BIN:
    {
        if (is_send_server_audio_over != "0")
        {
            is_send_server_audio_over = "0";
        }

        // 提取 session_id
        char sessionIdString[5];
        memcpy(sessionIdString, payload, 4);
        sessionIdString[4] = '\0';
        String sid = String(sessionIdString);

        if (sid == "2000" || sid == "2001")
        {
            JSONVar data;
            data["type"] = "client_receive_audio_over";
            data["session_id"] = session_id;
            data["sid"] = sid;
            String sendData = JSON.stringify(data);
            DEBUG_PRINTLN(debug, ("\n[TTS] -> 发送LLM播放结束标识到服务端"));
            webSocket.sendTXT(sendData);
            return;
        }

        if (sessionIdString && sid != "0000" && sid != session_id)
        {
            return;
        }
        // 提取音频数据
        uint8_t *audioData = payload + 4;
        size_t audioLength = length - 4;
        // Serial.print("length===");
        // Serial.println(audioLength);
        adjustVolume((int16_t *)audioData, audioLength, volume_config.volume);
        i2s.write(audioData, audioLength);
        break;
    }
    case WStype_PING:
        Serial.println("Ping");
        break;
    case WStype_PONG:
        Serial.println("Pong");
        break;
    case WStype_ERROR:
        Serial.println("[Error] 服务 WebSocket 连接错误");
        break;
    }
}
