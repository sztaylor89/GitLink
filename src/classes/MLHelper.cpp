#include "mathlink.h"
#include "git2.h"
#include "WolframLibrary.h"
#include "MLHelper.h"
#include "GitLinkRepository.h"
#include "Signature.h"
#include <ctime>

MLHelper::MLHelper(MLINK lnk) :
	lnk_(lnk), unfinishedRule_(false)
{
	tmpLinks_.push_front(lnk);
	argCounts_.push_front(0);
	unfinishedRule_.push_front(false);
}

MLHelper::MLHelper(MLEnvironment env, MLExpr& expr) :
	lnk_(expr.initializeLink(env)), unfinishedRule_(false)
{
	tmpLinks_.push_front(lnk_);
	argCounts_.push_front(0);
	unfinishedRule_.push_front(false);
}

MLHelper::~MLHelper()
{
	endAllFunctions();
}

void MLHelper::processAndIgnore(WolframLibraryData libData)
{
	endAllFunctions();
	libData->processWSLINK(lnk_);

	while (true)
	{
		switch(MLNextPacket(lnk_))
		{
			case ILLEGALPKT:	break;
			case RETURNPKT:		MLNewPacket(lnk_); break;
			default:			MLNewPacket(lnk_); continue;
		}
		break;
	}
}

void MLHelper::beginFunction(const char* head)
{
	int err;
	tmpLinks_.push_front(MLLoopbackOpen(MLLinkEnvironment(lnk_), &err));
	argCounts_.push_front(0);
	unfinishedRule_.push_front(false);
	MLPutSymbol(tmpLinks_.front(), head);
}

void MLHelper::beginFunction(const MLExpr& head)
{
	int err;
	tmpLinks_.push_front(MLLoopbackOpen(MLLinkEnvironment(lnk_), &err));
	argCounts_.push_front(0);
	unfinishedRule_.push_front(false);
	head.putToLink(tmpLinks_.front());
}

void MLHelper::endFunction()
{
	MLINK loopbackLink = tmpLinks_.front();
	int argCount = argCounts_.front();
	tmpLinks_.pop_front();
	argCounts_.pop_front();
	unfinishedRule_.pop_front();

	MLINK destLink = tmpLinks_.front();
	MLPutNext(destLink, MLTKFUNC);
	MLPutArgCount(destLink, argCount);
	MLTransferExpression(destLink, loopbackLink);
	for (int i = 0; i < argCount; i++)
		MLTransferExpression(destLink, loopbackLink);
	MLClose(loopbackLink);
	incrementArgumentCount_();
}

void MLHelper::endAllFunctions()
{
	while (tmpLinks_.front() != lnk_)
		endFunction();
}

void MLHelper::putString(const char* value)
{
	MLPutUTF8String(tmpLinks_.front(), (const unsigned char*)value, (int)strlen(value));
	incrementArgumentCount_();
}

void MLHelper::putSymbol(const char* value)
{
	MLPutSymbol(tmpLinks_.front(), value);
	incrementArgumentCount_();
}

void MLHelper::putMint(mint value)
{
	MLPutMint(tmpLinks_.front(), value);
	incrementArgumentCount_();
}

void MLHelper::putInt(int value)
{
	MLPutInteger(tmpLinks_.front(), value);
	incrementArgumentCount_();
}

void MLHelper::putOid(const git_oid& value)
{
	char buf[GIT_OID_HEXSZ + 1];
	git_oid_tostr(buf, GIT_OID_HEXSZ + 1, &value);
	MLPutString(tmpLinks_.front(), buf);
	incrementArgumentCount_();
}

void MLHelper::putRepo(const GitLinkRepository& repo)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "GitRepo", 1);
	repo.writeProperties(lnk, true);
	incrementArgumentCount_();
}

void MLHelper::putGitObject(const git_oid& value, const GitLinkRepository& repo)
{
	beginFunction("GitObject");
	putOid(value);
	putRepo(repo);
	endFunction();
}

void MLHelper::putExpr(const MLExpr& expr)
{
	MLINK lnk = tmpLinks_.front();
	expr.putToLink(lnk);
	incrementArgumentCount_();
}

void MLHelper::putMessage(const char* symbol, const char* tag)
{
	beginFunction("MessageName");
	putSymbol(symbol);
	putString(tag);
	endFunction();
}

void MLHelper::putBlobUTF8String(const git_blob* blob)
{
	MLINK lnk = tmpLinks_.front();
	MLPutUTF8String(lnk, (const unsigned char*)git_blob_rawcontent(blob), git_blob_rawsize(blob));
	incrementArgumentCount_();
}

void MLHelper::putBlobByteString(const git_blob* blob)
{
	MLINK lnk = tmpLinks_.front();
	MLPutByteString(lnk, (const unsigned char*)git_blob_rawcontent(blob), git_blob_rawsize(blob));
	incrementArgumentCount_();
}


void MLHelper::putRule(const char* key)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);
	argCounts_.front()++;
	unfinishedRule_.front() = true;
}

void MLHelper::putRule(const char* key, int value)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);
	MLPutSymbol(lnk, value ? "True" : "False");
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, double value)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);
	MLPutDouble(lnk, value);
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, const MLExpr& expr)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);
	expr.putToLink(lnk);
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, const git_time& value)
{
	struct tm* tmPtr = localtime((time_t*)&value.time);
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);
	MLPutFunction(lnk, "DateObject", 2);
	MLPutFunction(lnk, "List", 6);
	MLPutInteger(lnk, tmPtr->tm_year + 1900);
	MLPutInteger(lnk, tmPtr->tm_mon + 1);
	MLPutInteger(lnk, tmPtr->tm_mday);
	MLPutInteger(lnk, tmPtr->tm_hour);
	MLPutInteger(lnk, tmPtr->tm_min);
	MLPutInteger(lnk, tmPtr->tm_sec);
	MLPutFunction(lnk, "Rule", 2);
	MLPutSymbol(lnk, "TimeZone");
	MLPutReal(lnk, (double)value.offset / 60.);
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, const char* value, const char* symbolFallback)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutUTF8String(lnk, (const unsigned char*)key, (int)strlen(key));
	if (value == NULL)
		MLPutSymbol(lnk, symbolFallback);
	else
		MLPutUTF8String(lnk, (const unsigned char*)value, (int)strlen(value));
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, const git_oid& value)
{
	putRule(key);
	putOid(value);
}

void MLHelper::putRule(const char* key, const git_oid& value, const GitLinkRepository& repo)
{
	putRule(key);
	putGitObject(value, repo);
}

void MLHelper::putRule(const char* key, git_repository_state_t value)
{
	MLINK lnk = tmpLinks_.front();
	MLPutFunction(lnk, "Rule", 2);
	MLPutString(lnk, key);

	const char* state;
	switch (value)
	{
		case GIT_REPOSITORY_STATE_MERGE:
			state = "Merge";
			break;
		case GIT_REPOSITORY_STATE_REVERT:
			state = "Revert";
			break;
		case GIT_REPOSITORY_STATE_CHERRYPICK:
			state = "CherryPick";
			break;
		case GIT_REPOSITORY_STATE_BISECT:
			state = "Bisect";
			break;
		case GIT_REPOSITORY_STATE_REBASE:
			state = "Rebase";
			break;
		case GIT_REPOSITORY_STATE_REBASE_INTERACTIVE:
			state = "RebaseInteractive";
			break;
		case GIT_REPOSITORY_STATE_REBASE_MERGE:
			state = "RebaseMerge";
			break;
		case GIT_REPOSITORY_STATE_APPLY_MAILBOX:
			state = "ApplyMailbox";
			break;
		case GIT_REPOSITORY_STATE_APPLY_MAILBOX_OR_REBASE:
			state = "ApplyMailboxOrRebase";
			break;
		default:
			state = "None";
			break;
	}
	MLPutString(lnk, state);
	incrementArgumentCount_();
}

void MLHelper::putRule(const char* key, const Signature& value)
{
	putRule(key);
	value.writeAssociation(*this);
}

std::string MLGetCPPString(MLINK lnk)
{
	const unsigned char* bytes;
	std::string str;
	int len, unused;
	MLGetUTF8String(lnk, &bytes, &len, &unused);
	str.assign((const char*) bytes, len);
	MLReleaseUTF8String(lnk, bytes, len);
	return str;
}

void MLHandleError(WolframLibraryData libData, const char* functionName, const char* messageName, const char* param, const char* param2)
{
	if (messageName == NULL)
		return;

	MLINK lnk = libData->getMathLink(libData);
	MLPutFunction(lnk, "EvaluatePacket", 1);
	MLPutFunction(lnk, "Message", (param == NULL) ? 1 : ((param2 == NULL) ? 2 : 3));
	MLPutFunction(lnk, "MessageName", 2);
	MLPutSymbol(lnk, functionName);
	MLPutString(lnk, messageName);
	if (param)
	{
		MLPutString(lnk, param);
		if (param2)
			MLPutString(lnk, param2);
	}
	libData->processWSLINK(lnk);
	while (true)
	{
		switch(MLNextPacket(lnk))
		{
			case ILLEGALPKT:	return;
			case RETURNPKT:		MLNewPacket(lnk); return;
			default:			MLNewPacket(lnk); continue;
		}
	}
}

MLExpr MLToExpr(WolframLibraryData libData, const MLExpr& expr)
{
	MLINK lnk = libData->getMathLink(libData);
	MLHelper helper(lnk);

	helper.beginFunction("EvaluatePacket");
	helper.putExpr(expr);
	helper.endFunction();
	libData->processWSLINK(lnk);

	while (true)
	{
		switch(MLNextPacket(lnk))
		{
			case ILLEGALPKT:	return MLExpr();
			case RETURNPKT:		return MLExpr(lnk);
			default:			MLNewPacket(lnk);	continue;
		}
	}
}

std::string MLToLower(WolframLibraryData libData, const std::string& str)
{
	std::string result;

	MLINK lnk = libData->getMathLink(libData);
	MLPutFunction(lnk, "EvaluatePacket", 1);
	MLPutFunction(lnk, "ToLowerCase", 1);
	MLPutUTF8String(lnk, (const unsigned char*) str.c_str(), str.size());
	libData->processWSLINK(lnk);
	while (true)
	{
		switch(MLNextPacket(lnk))
		{
			case ILLEGALPKT:	return std::string();
			case RETURNPKT:		return MLGetCPPString(lnk);
			default:			MLNewPacket(lnk); continue;
		}
	}
}

const char* OtypeToString(git_otype otype)
{
	switch(otype)
	{
		case GIT_OBJ_COMMIT:	return "Commit";
		case GIT_OBJ_TREE:		return "Tree";
		case GIT_OBJ_BLOB:		return "Blob";
		case GIT_OBJ_TAG:		return "Tag";
		case GIT_OBJ_OFS_DELTA:	return "OffsetDelta";
		case GIT_OBJ_REF_DELTA:	return "ObjectDelta";
		default:				return "None";
	}
}