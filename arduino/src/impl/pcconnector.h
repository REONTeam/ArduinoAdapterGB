/*
	Controller -> PC bridge
	
	Implemented commands:
		Connect (id 1)
		Disconnect (id 0)
		Send (id 2)
		Receive (id 3)
	
	Data format (send):
		0xCD Length (2 bytes)
		0xCD CommandID Data (2+ bytes)
		
	Data format (recv):
		0xCD Data (if packet is not dynamic)
		0xCD Length Data (if packet is dynamic, like receiving network data)
		
	TODO: test if without the delay the transmission of a big page works
	TODO: Possibly test the most stable data chunk size for higher serial baud rates
*/

static void data_send(unsigned char command, const unsigned char* data, unsigned char len)
{
	unsigned char buf[270];
	
	delay(50);
	
	buf[0] = 0xCD;
	buf[1] = len;
	Serial.write(buf, 2);
	
	delay(50);
	
	buf[0] = 0xCD;
	buf[1] = command;
	
	if (len > 0)
		memcpy(buf + 2, data, len);
	
	Serial.write(buf, 2 + len);
}

static short data_recv(unsigned char* buf, unsigned char len, bool dyn)
{
	delay(50);
	
	bool done = false;
	int status = 0;
	unsigned int i = 0;
	unsigned char nlen = len;
	
	while (!done)
	{
		if (Serial.available())
		{
			unsigned char data = Serial.read();
			
			switch (status)
			{
				case 0: // Magic
					if (data == 0xCF) // Error packer
						return -1;
					else if (data != 0xCE)
						break;
					
					if (!dyn)
					{
						if (nlen < 1)
							done = true;
						
						status = 2;
					}
					else
						status = 1;
					break;
				case 1: // Dynamic packet length					
					nlen = data;
					status = 2;
					
					if (nlen < 1)
						done = true;
					break;
				case 2: // Data
					buf[i] = data;
					i++;
					
					if (i >= nlen)
						done = true;
					
					break;
			}
		}
	}
	
	return nlen;
}

bool mobile_board_tcp_connect(void *user, unsigned int conn, const unsigned char *host, const unsigned int port)
{
	unsigned char buf[7];
	buf[0] = conn & 0xFF;
	memcpy(buf + 1, host, 4);
	buf[5] = port >> 8;
	buf[6] = port & 0xFF;
		
	data_send(0x01, buf, 7);
	
	if (data_recv(buf, 1, false) != 1)
		return false;
	
	return buf[0] == 0x01;
}

bool mobile_board_tcp_listen(void *user, unsigned conn, const unsigned int port)
{
	// §todo
    return false;
}

bool mobile_board_tcp_accept(void *user, unsigned int conn)
{
	// §todo
    return false;
}

void mobile_board_tcp_disconnect(void *user, unsigned int conn)
{
	unsigned char conn2 = conn & 0xFF;
	data_send(0x00, &conn2, 1);
}

bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned int size)
{
	unsigned char buf[256];
	buf[0] = conn & 0xFF;
	buf[1] = size & 0xFF;
	memcpy(buf + 2, data, size);
	data_send(0x02, buf, size + 2);
	
	if (data_recv(buf, 1, false) != 1)
		return false;

	return buf[0] == 0x01;
}

int mobile_board_tcp_receive(void *user, unsigned int conn, void *data)
{
	unsigned char conn2 = conn & 0xFF;
	data_send(0x03, &conn2 , 1);
	
	return data_recv((unsigned char*)data, 0, true);
}
