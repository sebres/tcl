/* reserved for IrDA's IrLAP/LSAP sockets protocol */

#include "tclWinInt.h"
#include "tclWinsockCore.h"
#include <af_irda.h>


static Tcl_NetResolverProc ResolveIrDA;
static Tcl_NetDecodeAddrProc DecodeIrdaSockaddr;

static WS2ProtocolData irdaProtoData = {
    AF_IRDA,
    SOCK_STREAM,
    0,
    sizeof(SOCKADDR_IRDA),
    DecodeIrdaSockaddr,
    ResolveIrDA,	/* resolver */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateIrdaSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				int server, CONST char *myaddr,
				CONST char *myport, int async);


Tcl_Obj *
DecodeIrdaSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    char formatedId[12];
    Tcl_Obj *result = Tcl_NewObj();
    SOCKADDR_IRDA *irdaaddr = (SOCKADDR_IRDA *) addr;

    /* Device ID. */
    sprintf(formatedId, "%02x-%02x-%02x-%02x",
	    irdaaddr->irdaDeviceID[0], irdaaddr->irdaDeviceID[1],
	    irdaaddr->irdaDeviceID[2], irdaaddr->irdaDeviceID[3]);
    Tcl_ListObjAppendElement(NULL, result,
	    Tcl_NewStringObj(formatedId, 11));

    /* Service Name (probably not in UTF-8). */
    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(
	    irdaaddr->irdaServiceName, -1));

    return result;
}


void
ResolveIrDA (Tcl_NetResolverCmd command, Tcl_Obj *question, Tcl_Obj *argument)
{
    int result, objc;
    Tcl_Obj **objv;
    Tcl_Obj *answers = NULL;

    switch (command) {
	case NAME_QUERY:
	    /* asterix means "get all", aka discovery.. */
	    if (strcmp(Tcl_GetString(question), "*") == 0) {
		if (Do_IrDA_Discovery(&answers) != TCL_OK) {
		    /* error msg already sent. */
		    return;
		}
	    } else {
		result = Tcl_ListObjGetElements(NULL, argument, &objc, &objv);
		if (result == TCL_OK && objc == 2) {
		    if (Do_IrDA_Query(question, objv[0], objv[1], &answers) != TCL_OK) {
			/* error msg already sent. */
			return;
		    }
		}
	    }
	    break;
	case NAME_REGISTER:
	case NAME_UNREGISTER:
	    /* TODO */
	    break;
    }
    /* reply with answers */
    SendAnswers(question, answers);
    return;
}


int
Do_IrDA_Discovery (Tcl_Obj **answers)
{
    SOCKET sock;
    DEVICELIST *deviceListStruct;
    IRDA_DEVICE_INFO* thisDevice;
    int code, charSet, nameLen, size, limit;
    unsigned i, bit;
    char isocharset[] = "iso-8859-?", *nameEnc;
    Tcl_Encoding enc;
    Tcl_Obj* entry[3];
    const char *hints1[] = {
	"PnP", "PDA", "Computer", "Printer", "Modem", "Fax", "LAN", NULL
    };
    const char *hints2[] = {
	"Telephony", "Server", "Comm", "Message", "HTTP", "OBEX", NULL
    };
    char formatedId[12];
    Tcl_DString deviceDString;

    /* dunno... */
    limit = 20;

    /*
     * First make an IrDA socket.
     */

    sock = WSASocket(AF_IRDA, SOCK_STREAM, 0, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (sock == INVALID_SOCKET) {
	SendWinErrorData(407, "Cannot create IrDA socket", WSAGetLastError());
	return TCL_ERROR;
    }

    /*
     * Alloc the list we'll hand to getsockopt.
     */

    size = sizeof(DEVICELIST) - sizeof(IRDA_DEVICE_INFO)
	    + (sizeof(IRDA_DEVICE_INFO) * limit);
    deviceListStruct = (DEVICELIST *) ckalloc(size);
    deviceListStruct->numDevice = 0;

    code = getsockopt(sock, SOL_IRLMP, IRLMP_ENUMDEVICES,
	    (char*) deviceListStruct, &size);

    if (code == SOCKET_ERROR) {
	SendWinErrorData(408, "getsockopt() failed", WSAGetLastError());
	ckfree((char *)deviceListStruct);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */

    if (*answers == NULL) {
	*answers = Tcl_NewObj();
    }

    for (i = 0; i < deviceListStruct->numDevice; i++) {
	thisDevice = deviceListStruct->Device+i;
	sprintf(formatedId, "%02x-%02x-%02x-%02x",
		thisDevice->irdaDeviceID[0], thisDevice->irdaDeviceID[1],
		thisDevice->irdaDeviceID[2], thisDevice->irdaDeviceID[3]);
	entry[0] = Tcl_NewStringObj(formatedId, 11);
	charSet = (thisDevice->irdaCharSet) & 0xff;
	switch (charSet) {
	    case 0xff:
		nameEnc = "unicode"; break;
	    case 0:
		nameEnc = "ascii"; break;
	    default:
		nameEnc = isocharset; 
		isocharset[9] = charSet + '0';
		break;
	}
	enc = Tcl_GetEncoding(NULL, nameEnc);
	nameLen = (thisDevice->irdaDeviceName)[21] ? 22 :
		strlen(thisDevice->irdaDeviceName);
	Tcl_ExternalToUtfDString(enc, thisDevice->irdaDeviceName,
		nameLen, &deviceDString);
	Tcl_FreeEncoding(enc);
	entry[1] = Tcl_NewStringObj(Tcl_DStringValue(&deviceDString),
		Tcl_DStringLength(&deviceDString));
	Tcl_DStringFree(&deviceDString);
	entry[2] = Tcl_NewObj();
	for (bit=0; hints1[bit]; ++bit) {
	    if (thisDevice->irdaDeviceHints1 & (1<<bit))
		Tcl_ListObjAppendElement(NULL, entry[2],
			Tcl_NewStringObj(hints1[bit],-1));
	}
	for (bit=0; hints2[bit]; ++bit) {
	    if (thisDevice->irdaDeviceHints2 & (1<<bit))
		Tcl_ListObjAppendElement(NULL, entry[2],
			Tcl_NewStringObj(hints2[bit],-1));
	}
	Tcl_ListObjAppendElement(NULL, *answers,
		Tcl_NewListObj(3, entry));
    }

    ckfree((char *)deviceListStruct);
    closesocket(sock);

    return TCL_OK;
}

int
Do_IrDA_Query (Tcl_Obj *deviceId, Tcl_Obj *serviceName,
	Tcl_Obj *attribName, Tcl_Obj **answers)
{
    SOCKET sock;
    int code, size = sizeof(IAS_QUERY);
    IAS_QUERY iasQuery;

    /*
     * Decode irdaDeviceId
     */
    code = sscanf(Tcl_GetString(deviceId), "%02x-%02x-%02x-%02x",
	&iasQuery.irdaDeviceID[0], &iasQuery.irdaDeviceID[1],
	&iasQuery.irdaDeviceID[2], &iasQuery.irdaDeviceID[3]);
    if (code != 4) {
	SendProtocolError(409, "Malformed IrDA DeviceID.  Must be in the form \"FF-FF-FF-FF.\"");
	return TCL_ERROR;
    }

    /*
     * First, make an IrDA socket.
     */

    sock = socket(AF_IRDA, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
	SendWinErrorData(407, "Cannot create IrDA socket", WSAGetLastError());
	return TCL_ERROR;
    }

    strncpy(iasQuery.irdaAttribName, Tcl_GetString(attribName), 256);
    strncpy(iasQuery.irdaClassName, Tcl_GetString(serviceName), 64);

    code = getsockopt(sock, SOL_IRLMP, IRLMP_IAS_QUERY,
	    (char*) &iasQuery, &size);

    if (code == SOCKET_ERROR) {
	if (WSAGetLastError() != WSAECONNREFUSED) {
	    SendWinErrorData(408, "getsockopt() failed", WSAGetLastError());
	} else {
	    SendProtocolError(410, "No such service.");
	}
	closesocket(sock);
	return TCL_ERROR;
    }

    /*
     * Create the output Tcl_Obj, if none exists there.
     */

    if (*answers == NULL) {
	*answers = Tcl_NewObj();
    }

    closesocket(sock);

    switch (iasQuery.irdaAttribType) {
	case IAS_ATTRIB_INT:
	    Tcl_SetIntObj(*answers, iasQuery.irdaAttribute.irdaAttribInt);
	    return TCL_OK;
	case IAS_ATTRIB_OCTETSEQ:
	    Tcl_SetByteArrayObj(*answers, iasQuery.irdaAttribute.irdaAttribOctetSeq.OctetSeq,
		    iasQuery.irdaAttribute.irdaAttribOctetSeq.Len);
	    return TCL_OK;
	case IAS_ATTRIB_STR: {
		Tcl_Encoding enc;
		char isocharset[] = "iso-8859-?", *nameEnc;
		int charSet = iasQuery.irdaAttribute.irdaAttribUsrStr.CharSet  & 0xff;
		Tcl_DString deviceDString;
		switch (charSet) {
		    case 0xff:
			nameEnc = "unicode";
			break;
		    case 0:
			nameEnc = "ascii";
			break;
		    default:
			nameEnc = isocharset; 
			isocharset[9] = charSet + '0';
			break;
		}
		enc = Tcl_GetEncoding(NULL, nameEnc);
		Tcl_ExternalToUtfDString(enc, iasQuery.irdaAttribute.irdaAttribUsrStr.UsrStr,
			iasQuery.irdaAttribute.irdaAttribUsrStr.Len, &deviceDString);
		Tcl_FreeEncoding(enc);
		Tcl_SetStringObj(*answers, Tcl_DStringValue(&deviceDString),
			Tcl_DStringLength(&deviceDString));
		Tcl_DStringFree(&deviceDString);
	    }
	    return TCL_OK;
	case IAS_ATTRIB_NO_CLASS:
	    Tcl_SetStringObj(*answers, "no such class", -1);
	    return TCL_OK;
	case IAS_ATTRIB_NO_ATTRIB:
	    Tcl_SetStringObj(*answers, "no such attribute", -1);
	    return TCL_OK;
	default:
	    Tcl_Panic("No such arm.");
	    return TCL_ERROR;  /* makes compiler happy */
    }
}
