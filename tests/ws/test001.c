// send/receive same data
// 1. send/recv 1 frame of data
// 2. send/recv multiple frames of data
// 3. masked data
// 4. close frames

static int on_open_server = 0;
static int send_basic_server = 0;
static int send_multiple_server = 0;
static int send_masked_server = 0;
static int send_close_server = 0;

static int on_open_client = 0;
static int send_basic_client = 0;
static int send_multiple_client = 0;
static int send_masked_client = 0;
static int send_close_client = 0;