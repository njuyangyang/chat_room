#ifndef SBCP_Struct_H
#define SBCP_Struct_H

//SBCP Header
struct SBCP_Header{
	unsigned int vrsn : 9;
	unsigned int type : 7;
	unsigned int length: 16;
};


//SBCP Attribute
struct SBCP_Attribute{
	unsigned int type : 16;
	unsigned int length : 16;
	char content[512];
};


//SBCP Message
struct SBCP_Message{
	struct SBCP_Header head;
	struct SBCP_Attribute attribute[2];
};

//client information
struct SBCP_Information{
	char username[16];
	int fd;
	struct SBCP_Information* front;
	struct SBCP_Information* following;
};

#endif

