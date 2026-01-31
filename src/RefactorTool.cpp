#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <clang-c/Index.h>
#include <clang/AST/Attrs.inc>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/StmtCXX.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeLoc.h>
#include <clang/Basic/AttrKinds.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <llvm-20/llvm/ADT/DenseSet.h>
#include <llvm-20/llvm/Support/Casting.h>
#include <llvm-20/llvm/Support/Errc.h>
#include <unordered_set>

#include "RefactorTool.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

// Метод run вызывается для каждого совпадения с матчем.
// Мы проверяем тип совпадения по bind-именам и применяем рефакторинг.
void RefactorHandler::run(const MatchFinder::MatchResult &Result) {
    auto &Diag = Result.Context->getDiagnostics();
    auto &SM = *Result.SourceManager;  // Получаем SourceManager для проверки isInMainFile

    if (const auto *Dtor = Result.Nodes.getNodeAs<CXXDestructorDecl>("nonVirtualDtor")) {
        handle_nv_dtor(Dtor, Diag, SM);
    }

    if (const auto *Method = Result.Nodes.getNodeAs<CXXMethodDecl>("missingOverride")) {
        handle_miss_override(Method, Diag, SM);
    }

    if (const auto *LoopVar = Result.Nodes.getNodeAs<VarDecl>("loopVar")) {
        handle_crange_for(LoopVar, Diag, SM);
    }
}

// todo: необходимо реализовать обработку случая невиртуального деструктора
void RefactorHandler::handle_nv_dtor(const CXXDestructorDecl *Dtor, DiagnosticsEngine &Diag, SourceManager &SM) {
    if (!SM.isInMainFile(Dtor->getLocation())) {
        return;
    }

    const CXXRecordDecl *Base = Dtor->getParent();
    Base = Base->getCanonicalDecl();

    for (const Decl *D : Base->getTranslationUnitDecl()->decls()) {
        const auto *CRD = dyn_cast<CXXRecordDecl>(D);
        if (!CRD || !CRD->hasDefinition()) {
            continue;
        }

        for (const auto &BaseSpec : CRD->bases()) {
            const CXXRecordDecl *B = BaseSpec.getType()->getAsCXXRecordDecl();
            if (!B)
                continue;
            if (B->getCanonicalDecl() == Base) {
                if (virtualDtorLocations.count(Dtor->getLocation().getRawEncoding()) == 0) {
                    virtualDtorLocations.insert(Dtor->getLocation().getRawEncoding());

                    Rewrite.InsertTextBefore(Dtor->getBeginLoc(), "virtual ");

                    const unsigned DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Remark, "Handled non-virtual dtor");
                    Diag.Report(Dtor->getLocation(), DiagID);
                }
            }
        }
    }
}

// todo: необходимо реализовать обработку случая отсутствие override
void RefactorHandler::handle_miss_override(const CXXMethodDecl *Method, DiagnosticsEngine &Diag, SourceManager &SM) {
    if (!SM.isInMainFile(Method->getLocation())) {
        return;
    }

    const TypeSourceInfo *TSI = Method->getTypeSourceInfo();
    if (!TSI) {
        return;
    }

    FunctionProtoTypeLoc FPTL = TSI->getTypeLoc().getAs<FunctionProtoTypeLoc>();
    SourceLocation AfterParen = FPTL.getRParenLoc().getLocWithOffset(1);

    if (AfterParen.isValid()) {
        Rewrite.InsertTextAfter(AfterParen, " override");
    }

    const unsigned DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Remark, "Handled miss override");
    Diag.Report(Method->getLocation(), DiagID);
}

// todo: необходимо реализовать обработку случая отсутствие & в range-for
void RefactorHandler::handle_crange_for(const VarDecl *LoopVar, DiagnosticsEngine &Diag, SourceManager &SM) {
    if (!SM.isInMainFile(LoopVar->getLocation())) {
        return;
    }

    QualType QT = LoopVar->getType();
    if (QT->isReferenceType() || QT->isFundamentalType()) {
        return;
    }

    const auto InsertLoc = LoopVar->getLocation();
    Rewrite.InsertTextAfter(InsertLoc, "& ");
    const unsigned DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Remark, "Handled range for");
    Diag.Report(LoopVar->getLocation(), DiagID);
}

// todo: ниже необходимо реализовать матчеры для поиска узлов AST
// note: синтаксис написания матчеров точно такой же как и для использования clang-query
/*
    Пример того, как может выглядеть реализация:
    auto AllClassesMatcher()
    {
        return cxxRecordDecl().bind("classDecl");
    }
*/
auto NvDtorMatcher() {
    // clang-format off
    return cxxDestructorDecl(
        unless(isVirtual()),
        unless(isImplicit()),
        ofClass(
            cxxRecordDecl(
                isDefinition()
            )
        )
    ).bind("nonVirtualDtor");
    // clang-format on
}

auto NoOverrideMatcher() {
    // clang-format off
    return cxxMethodDecl(
        isOverride(),
        unless(hasAttr(attr::Override)),
        unless(isImplicit()),
        unless(cxxDestructorDecl())
    ).bind("missingOverride");
    // clang-format on
}

auto NoRefConstVarInRangeLoopMatcher() {
    // clang-format off
    return cxxForRangeStmt(
        hasLoopVariable(
            varDecl(
                hasType(isConstQualified()),
                unless(hasType(referenceType()))
            ).bind("loopVar")
        )
    );
    // clang-format on
}

// Конструктор принимает Rewriter для изменения кода.
ComplexConsumer::ComplexConsumer(Rewriter &Rewrite) : Handler(Rewrite) {
    // Создаем MatchFinder и добавляем матчеры.
    Finder.addMatcher(NvDtorMatcher(), &Handler);
    Finder.addMatcher(NoOverrideMatcher(), &Handler);
    Finder.addMatcher(NoRefConstVarInRangeLoopMatcher(), &Handler);
}

// Метод HandleTranslationUnit вызывается для каждого файла.
void ComplexConsumer::HandleTranslationUnit(ASTContext &Context) { Finder.matchAST(Context); }

std::unique_ptr<ASTConsumer> CodeRefactorAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
    RewriterForCodeRefactor.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<ComplexConsumer>(RewriterForCodeRefactor);
}

bool CodeRefactorAction::BeginSourceFileAction(CompilerInstance &CI) {
    // Инициализируем Rewriter для рефакторинга.
    RewriterForCodeRefactor.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return true;  // Возвращаем true, чтобы продолжить обработку файла.
}

void CodeRefactorAction::EndSourceFileAction() {
    // Применяем изменения в файле.
    if (RewriterForCodeRefactor.overwriteChangedFiles()) {
        llvm::errs() << "Error applying changes to files.\n";
    }
}
