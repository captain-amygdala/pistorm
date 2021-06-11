#pragma pack(push, 1)

struct LocateObjectRequest
{
	short has_response;
	short type;
	long key;
	short mode;
	char name[1];
};

struct LocateObjectResponse
{
	short has_response;
	short success;
	short error_code;
	long key;
};

struct FreeLockRequest
{
	short has_response;
	short type;
	long key;
};

struct FreeLockResponse
{
	short has_response;
	short success;
	short error_code;
};

struct CopyDirRequest
{
	short has_response;
	short type;
	long key;
};

struct CopyDirResponse
{
	short has_response;
	short success;
	short error_code;
	long key;
};

struct ParentRequest
{
	short has_response;
	short type;
	long key;
};

struct ParentResponse
{
	short has_response;
	short success;
	short error_code;
	long key;
};

struct ExamineObjectRequest
{
	short has_response;
	short type;
	long key;
};

struct ExamineObjectResponse
{
	short has_response;
	short success;
	short error_code;

	short disk_key;
	short entry_type;
	int size;
	int protection;
	int date[3];
	char data[1];
};

struct ExamineNextRequest
{
	short has_response;
	short type;
	long key;
	short disk_key;
};

struct ExamineNextResponse
{
	short has_response;
	short success;
	short error_code;

	short disk_key;
	short entry_type;
	int size;
	int protection;
	int date[3];
	char data[1];
};

struct FindXxxRequest
{
	short has_response;
	short type;
	long key;
	char name[1];
};

struct FindXxxResponse
{
	short has_response;
	short success;
	short error_code;
	long arg1;
};

struct ReadRequest
{
	short has_response;
	short type;
	long arg1;
	int address;
	int length;
};

struct ReadResponse
{
	short has_response;
	short success;
	short error_code;
	int actual;
};

struct WriteRequest
{
	short has_response;
	short type;
	long arg1;
	int address;
	int length;
};

struct WriteResponse
{
	short has_response;
	short success;
	short error_code;
	int actual;
};

struct SeekRequest
{
	short has_response;
	short type;
	long arg1;
	int new_pos;
	int mode;
};

struct SeekResponse
{
	short has_response;
	short success;
	short error_code;
	int old_pos;
};

struct EndRequest
{
	short has_response;
	short type;
	long arg1;
};

struct EndResponse
{
	short has_response;
	short success;
	short error_code;
};

struct DeleteObjectRequest
{
	short has_response;
	short type;
	long key;
	char name[1];
};

struct DeleteObjectResponse
{
	short has_response;
	short success;
	short error_code;
};

struct RenameObjectRequest
{
	short has_response;
	short type;
	long key;
	long target_dir;
	unsigned char name_len;
	unsigned char new_name_len;
};

struct RenameObjectResponse
{
	short has_response;
	short success;
	short error_code;
};

struct CreateDirRequest
{
	short has_response;
	short type;
	long key;
	char name[1];
};

struct CreateDirResponse
{
	short has_response;
	short success;
	short error_code;
	long key;
};

struct SetProtectRequest
{
	short has_response;
	short type;
	long key;
	long mask;
	char name[1];
};

struct SetProtectResponse
{
	short has_response;
	short success;
	short error_code;
};

struct SetCommentRequest
{
	short has_response;
	short type;
	long key;
	unsigned char name_len;
	unsigned char comment_len;
};

struct SetCommentResponse
{
	short has_response;
	short success;
	short error_code;
};

struct SameLockRequest
{
	short has_response;
	short type;
	long key1;
	long key2;
};

struct SameLockResponse
{
	short has_response;
	short success;
	short error_code;
};

struct ExamineFhRequest
{
	short has_response;
	short type;
	long arg1;
};

struct ExamineFhResponse
{
	short has_response;
	short success;
	short error_code;

	short disk_key;
	short entry_type;
	int size;
	int protection;
	int date[3];
	char data[1];
};

struct UnsupportedRequest
{
	short has_response;
	short type;
	short dp_Type;
};

struct UnsupportedResponse
{
	short has_response;
	short success;
	short error_code;
};

#pragma pack(pop)
