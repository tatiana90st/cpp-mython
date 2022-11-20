#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <algorithm>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (object) {
        if (const Bool* ptr = object.TryAs<Bool>();
            ptr != nullptr) {
            return ptr->GetValue();
        }
        else if (const String* ptr = object.TryAs<String>();
            ptr != nullptr) {
            return !ptr->GetValue().empty();
        }
        else if (const Number* ptr = object.TryAs<Number>();
            ptr != nullptr) {
            return ptr->GetValue() != 0;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

void ClassInstance::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    if (HasMethod("__str__"s, 0)) {
        ObjectHolder object = Call("__str__"s, {}, context);
        object.Get()->Print(os, context);
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* m = cls_.GetMethod(method);
    if (m) {
        return m->formal_params.size() == argument_count;
    }
    else {
        return false;
    }
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls)
    :cls_(cls)
{
}

ObjectHolder ClassInstance::Call(const std::string& method,
    const std::vector<ObjectHolder>& actual_args,
    [[maybe_unused]] Context& context) {

    if (HasMethod(method, actual_args.size())) {
        Closure closure;
        closure["self"s] = ObjectHolder::Share(*this);

        const Method* m = cls_.GetMethod(method);
        for (size_t i = 0; i < actual_args.size(); ++i) {
            closure[m->formal_params[i]] = actual_args[i];
        }

        return m->body.get()->Execute(closure, context);
    }
    else {
        throw std::runtime_error("Method not found"s);
    }

}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    :name_(std::move(name))
    , methods_(std::move(methods))
    , parent_(parent)
{
}

const Method* Class::GetMethod(const std::string& name) const {
    auto comp = [&name](const Method& m) {
        return m.name == name;
    };
    auto m_ptr = std::find_if(methods_.begin(), methods_.end(), comp);
    if (m_ptr != methods_.end()) {
        return &(*m_ptr);
    }
    else {
        if (parent_) {
            return parent_->GetMethod(name);
        }
        else {
            return nullptr;
        }
    }
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "sv << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

struct Eq {
    template <typename T>
    bool operator()(const T& lhs, const T& rhs) {
        return lhs == rhs;
    }
};

struct Le {
    template <typename T>
    bool operator()(const T& lhs, const T& rhs) {
        return lhs < rhs;
    }
};

template <typename Comp>
bool CompareValueObjects(const ObjectHolder& lhs, const ObjectHolder& rhs, Comp comp) {
    auto bool_l = lhs.TryAs<Bool>();
    auto bool_r = rhs.TryAs<Bool>();
    if (bool_l) {
        if (bool_r) {
            return comp(bool_l->GetValue(), bool_r->GetValue());
        }
        else {
            throw std::runtime_error("Different types of compared objects"s);
        }
    }
    auto num_l = lhs.TryAs<Number>();
    auto num_r = rhs.TryAs<Number>();
    if (num_l) {
        if (num_r) {
            return comp(num_l->GetValue(), num_r->GetValue());
        }
        else {
            throw std::runtime_error("Different types of compared objects"s);
        }
    }
    auto str_l = lhs.TryAs<String>();
    auto str_r = rhs.TryAs<String>();
    if (str_l) {
        if (str_r) {
            return comp(str_l->GetValue(), str_r->GetValue());
        }
        else {
            throw std::runtime_error("Different types of compared objects"s);
        }
    }
    throw std::runtime_error("Comparation error"s);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    
    if (!lhs && !rhs) {
        return true;
    }
    if (ClassInstance* ptr = lhs.TryAs<ClassInstance>();
        ptr != nullptr) {
        if (ptr->HasMethod("__eq__"s, 1)) {
            Bool* res = ptr->Call("__eq__"s, { rhs }, context).TryAs<Bool>();
            if (res) {
                return res->GetValue();
            }
        }
    }
    else {
        return CompareValueObjects(lhs, rhs, Eq());
    }
    throw std::runtime_error("Comparation error"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

    if (ClassInstance* ptr = lhs.TryAs<ClassInstance>();
        ptr != nullptr) {
        if (ptr->HasMethod("__lt__"s, 1)) {
            Bool* res = ptr->Call("__lt__"s, { rhs }, context).TryAs<Bool>();
            if (res) {
                return res->GetValue();
            }
        }
    }
    else {
        return CompareValueObjects(lhs, rhs, Le());
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime