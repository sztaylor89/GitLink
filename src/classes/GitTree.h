#ifndef GitTree_h_
#define GitTree_h_ 1

class GitTree : public GitLinkSuperClass
{
public:
	GitTree(const GitLinkRepository& repo, git_index* index);
	GitTree(const MLExpr& expr);
	GitTree(MLINK lnk)
		: GitTree(MLExpr(lnk))
	{ };

	virtual ~GitTree();

	void write(MLINK lnk) const;
	void writeContents(MLINK lnk, int depth) const;

	bool isValid() const { return tree_ != NULL; };
	const git_oid* oid() const { return &oid_; };
	operator const git_tree*() const {return tree_; };

private:
	const GitLinkRepository repo_;
	git_tree* tree_ = NULL;
	git_oid oid_;
	mutable MLHelper* helper_ = NULL; // used for callbacks
	mutable int depth_ = 1; // used for callbacks

	static int writeTreeEntry(const char* root, const git_tree_entry* entry, void* payload);
};

#endif // GitTree_h_