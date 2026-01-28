#include <gtest/gtest.h>

#include "RefactorTool.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace clang::tooling;

class TestRefactorAction : public CodeRefactorAction {
public:
    explicit TestRefactorAction(std::string &Out) : Output(Out) {}

    void EndSourceFileAction() override {
        const SourceManager &SM = RewriterForCodeRefactor.getSourceMgr();

        FileID MainFileID = SM.getMainFileID();

        const llvm::RewriteBuffer &RewriteBuf = RewriterForCodeRefactor.getEditBuffer(MainFileID);

        Output.assign(RewriteBuf.begin(), RewriteBuf.end());
    }

private:
    std::string &Output;
};

static std::string runTool(const std::string &Code) {
    std::string Result;
    auto Action = std::make_unique<TestRefactorAction>(Result);

    EXPECT_TRUE(runToolOnCodeWithArgs(std::move(Action), Code, {"-std=c++17"}));

    return Result;
}

TEST(Refactor, AddVirtualDestructor) {
    const char *Input = R"cpp(
        struct Base {
            ~Base();
        };

        struct Derived : Base {};
    )cpp";

    const char *Expected = R"cpp(
        struct Base {
            virtual ~Base();
        };

        struct Derived : Base {};
    )cpp";

    std::string Output = runTool(Input);
    EXPECT_EQ(Output, Expected);
}

TEST(Refactor, AddOverrideSpecifier) {
    const char *Input = R"cpp(
        struct Base {
            virtual void foo();
        };
        
        struct Derived : Base {
            void foo();
        };
    )cpp";

    const char *Expected = R"cpp(
        struct Base {
            virtual void foo();
        };
    
        struct Derived : Base {
            void foo() override;
        };
    )cpp";

    std::string Output = runTool(Input);
    EXPECT_EQ(Output, Expected);
}

TEST(Refactor, FixConstRangeFor) {
    const char *Input = R"cpp(
        #include <vector>

        void f(const std::vector<int> &v) {
            for (const int x : v) {
                (void)x;
            }
        }
    )cpp";

    const char *Expected = R"cpp(
        #include <vector>
        
        void f(const std::vector<int> &v) {
            for (const int& x : v) {
                (void)x;
            }
        }
    )cpp";

    std::string Output = runTool(Input);
    EXPECT_EQ(Output, Expected);
}