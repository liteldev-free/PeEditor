#include "pe_editor/FakeSymbol.h"

#include "demangler/MicrosoftDemangle.h"

bool consume_front(std::string_view& str, std::string_view substr) {
    if (str.starts_with(substr)) {
        str.remove_prefix(substr.size());
        return true;
    }
    return false;
}

namespace pe_editor::FakeSymbol {

inline demangler::ms_demangle::SpecialIntrinsicKind consumeSpecialIntrinsicKind(std::string_view& MangledName) {
    using namespace demangler::ms_demangle;
    using namespace demangler;
    if (consume_front(MangledName, "?_7")) return SpecialIntrinsicKind::Vftable;
    if (consume_front(MangledName, "?_8")) return SpecialIntrinsicKind::Vbtable;
    if (consume_front(MangledName, "?_9")) return SpecialIntrinsicKind::VcallThunk;
    if (consume_front(MangledName, "?_A")) return SpecialIntrinsicKind::Typeof;
    if (consume_front(MangledName, "?_B")) return SpecialIntrinsicKind::LocalStaticGuard;
    if (consume_front(MangledName, "?_C")) return SpecialIntrinsicKind::StringLiteralSymbol;
    if (consume_front(MangledName, "?_P")) return SpecialIntrinsicKind::UdtReturning;
    if (consume_front(MangledName, "?_R0")) return SpecialIntrinsicKind::RttiTypeDescriptor;
    if (consume_front(MangledName, "?_R1")) return SpecialIntrinsicKind::RttiBaseClassDescriptor;
    if (consume_front(MangledName, "?_R2")) return SpecialIntrinsicKind::RttiBaseClassArray;
    if (consume_front(MangledName, "?_R3")) return SpecialIntrinsicKind::RttiClassHierarchyDescriptor;
    if (consume_front(MangledName, "?_R4")) return SpecialIntrinsicKind::RttiCompleteObjLocator;
    if (consume_front(MangledName, "?_S")) return SpecialIntrinsicKind::LocalVftable;
    if (consume_front(MangledName, "?__E")) return SpecialIntrinsicKind::DynamicInitializer;
    if (consume_front(MangledName, "?__F")) return SpecialIntrinsicKind::DynamicAtexitDestructor;
    if (consume_front(MangledName, "?__J")) return SpecialIntrinsicKind::LocalStaticThreadGuard;
    return SpecialIntrinsicKind::None;
}

// generate fakeSymbol for virtual functions
std::optional<std::string> getFakeSymbol(const std::string& fn, bool removeVirtual) {
    using namespace demangler::ms_demangle;
    using namespace demangler;
    Demangler        demangler;
    std::string_view name(fn.c_str());

    if (!consume_front(name, "?")) return std::nullopt;

    SpecialIntrinsicKind specialIntrinsicKind = consumeSpecialIntrinsicKind(name);

    if (specialIntrinsicKind != SpecialIntrinsicKind::None) return std::nullopt;

    SymbolNode* symbolNode = demangler.demangleDeclarator(name);
    if (symbolNode == nullptr
        || (symbolNode->kind() != NodeKind::FunctionSymbol && symbolNode->kind() != NodeKind::VariableSymbol)
        || demangler.Error)
        return std::nullopt;

    if (symbolNode->kind() == NodeKind::FunctionSymbol) {
        auto&  funcNode     = reinterpret_cast<FunctionSymbolNode*>(symbolNode)->Signature->FunctionClass;
        bool   modified     = false;
        size_t funcNodeSize = funcNode.toString().size();
        if (removeVirtual) {
            if (funcNode.has(FC_Virtual)) {
                funcNode.remove(FC_Virtual);
                modified = true;
            } else {
                return std::nullopt;
            }
        }
        if (funcNode.has(FC_Protected)) {
            funcNode.remove(FC_Protected);
            funcNode.add(FC_Public);
            modified = true;
        }
        if (funcNode.has(FC_Private)) {
            funcNode.remove(FC_Private);
            funcNode.add(FC_Public);
            modified = true;
        }
        if (modified) {
            std::string fakeSymbol   = fn;
            std::string fakeFuncNode = funcNode.toString();
            size_t      offset       = fn.size() - funcNode.pos.size();
            fakeSymbol.erase(offset, funcNodeSize);
            fakeSymbol.insert(offset, fakeFuncNode);
            return fakeSymbol;
        }
    } else if (symbolNode->kind() == NodeKind::VariableSymbol) {
        auto& storageClass = reinterpret_cast<VariableSymbolNode*>(symbolNode)->SC;
        bool  modified     = false;
        if (storageClass == StorageClass::PrivateStatic) {
            storageClass.set(StorageClass::PublicStatic);
            modified = true;
        }
        if (storageClass == StorageClass::ProtectedStatic) {
            storageClass.set(StorageClass::PublicStatic);
            modified = true;
        }
        if (modified) {
            std::string fakeSymbol       = fn;
            char        fakeStorageClass = storageClass.toChar();
            size_t      offset           = fn.size() - storageClass.pos.size();
            fakeSymbol[offset]           = fakeStorageClass;
            return fakeSymbol;
        }
    }

    return std::nullopt;
}

} // namespace pe_editor::FakeSymbol
