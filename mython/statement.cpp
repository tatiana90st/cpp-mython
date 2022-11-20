#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_.get()->Execute(closure, context);
    return closure.at(var_);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) 
    :var_(std::move(var))
    ,rv_(std::move(rv))
{
}

VariableValue::VariableValue(const std::string& var_name) 
    :var_name_(std::move(var_name))
{
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) 
    :var_name_(dotted_ids[0])
{
    if (dotted_ids.size() > 1) {
        dotted_ids_.reserve(dotted_ids.size() - 1);
        for (size_t i = 1; i < dotted_ids.size(); ++i) {
            dotted_ids_.push_back(std::move(dotted_ids[i]));
        }
    }
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
    if (closure.count(var_name_)) {
        runtime::ObjectHolder obj = closure.at(var_name_);
        if (dotted_ids_.size()>0) {
            auto class_inst_ptr_ = obj.TryAs<runtime::ClassInstance>();
            if (class_inst_ptr_) {
                return VariableValue(dotted_ids_).Execute(class_inst_ptr_->Fields(), context);
            }
            else {
                //throw something?
            }
        }
        return obj;
    }
    else {
        throw std::runtime_error("Unknown variable name"s);
    }
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return std::make_unique<Print>(std::make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) 
    :args_(std::move(args))
{
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    bool first = true;
    std::ostream& out = context.GetOutputStream();
    for (auto& var : args_) {
        if (!first) {
            out << " "s;
        }
        runtime::ObjectHolder object = var.get()->Execute(closure, context);
        runtime::Object* obj_ptr_ = object.Get();
        if (obj_ptr_) {
            obj_ptr_->Print(out, context);
        }
        else {
            out << "None"s;
        }
        first = false;
    }
    out << "\n"s;
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
    std::vector<std::unique_ptr<Statement>> args) 
    :object_(std::move(object))
    ,method_(std::move(method))
    ,args_(std::move(args))
{
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
      ObjectHolder obj = object_.get()->Execute(closure, context);
      runtime::ClassInstance* obj_cl = obj.TryAs<runtime::ClassInstance>();
      if (obj_cl) {
          std::vector<runtime::ObjectHolder> args(args_.size());
          for (size_t i = 0; i < args.size(); ++i) {
              args[i] = args_[i].get()->Execute(closure, context);
          }
          return obj_cl->Call(method_, args, context);
      }
      else {
          throw std::runtime_error("Object must be a ClassInstance to call a method"s);
      }
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder object = argument_.get()->Execute(closure, context);
    if (object) {
        std::ostringstream to_string;
        object->Print(to_string, context);
        return ObjectHolder::Own(runtime::String(to_string.str()));
        //ObjectHolder<-Object::String<-stream to string<-Print to stream<-ObjectHolder<-Execute<-unique_ptr<-Statement==Executable
    }
    else {
        return ObjectHolder::Own(runtime::String("None"s));
    }
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
    auto left_ptr = left_obj.TryAs<runtime::Number>();
    auto right_ptr = right_obj.TryAs<runtime::Number>();
    if (left_ptr && right_ptr) {
        return runtime::ObjectHolder::Own(runtime::Number(left_ptr->GetValue() + right_ptr->GetValue()));
    }
    auto left_ptr_str = left_obj.TryAs<runtime::String>();
    auto right_ptr_str = right_obj.TryAs<runtime::String>();
    if (left_ptr_str && right_ptr_str) {
        return runtime::ObjectHolder::Own(runtime::String(left_ptr_str->GetValue() + right_ptr_str->GetValue()));
    }
    auto left_ptr_class = left_obj.TryAs<runtime::ClassInstance>();
    if (left_ptr_class) {
        return left_ptr_class->Call(ADD_METHOD, { right_obj }, context);
    }
    throw std::runtime_error("Failed to add, check arguments"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
    auto left_ptr = left_obj.TryAs<runtime::Number>();
    auto right_ptr = right_obj.TryAs<runtime::Number>();
    if (left_ptr && right_ptr) {
        return runtime::ObjectHolder::Own(runtime::Number(left_ptr->GetValue() - right_ptr->GetValue()));
    }
    throw std::runtime_error("Failed to sub, check arguments"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
    auto left_ptr = left_obj.TryAs<runtime::Number>();
    auto right_ptr = right_obj.TryAs<runtime::Number>();
    if (left_ptr && right_ptr) {
        return runtime::ObjectHolder::Own(runtime::Number(left_ptr->GetValue() * right_ptr->GetValue()));
    }
    throw std::runtime_error("Failed to mult, check arguments"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    runtime::ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
    auto left_ptr = left_obj.TryAs<runtime::Number>();
    auto right_ptr = right_obj.TryAs<runtime::Number>();
    if (left_ptr && right_ptr) {
        if (right_ptr->GetValue() == 0) {
            throw std::runtime_error("Failed to divide by 0, can't deal with eternity"s);
        }
        return runtime::ObjectHolder::Own(runtime::Number(left_ptr->GetValue() / right_ptr->GetValue()));
    }
    throw std::runtime_error("Failed to div, check arguments"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (auto& argument : args_) {
        argument.get()->Execute(closure, context);
    }
    return ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_.get()->Execute(closure, context);
}

ClassDefinition::ClassDefinition(ObjectHolder cls) 
    :cls_(std::move(cls))
{   
}

ObjectHolder ClassDefinition::Execute(Closure& closure,[[maybe_unused]] Context& context) {
    runtime::Class* cl_ptr = cls_.TryAs<runtime::Class>();
    if (cl_ptr) {
        closure[cl_ptr->GetName()] = std::move(cls_);
        return ObjectHolder::None();
    }
    else {
        throw std::runtime_error("ClassDefinition error of some kind"s);
    }
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
    std::unique_ptr<Statement> rv) 
    :object_(std::move(object))
    ,field_name_(std::move(field_name))
    ,rv_(std::move(rv))
{
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    runtime::ObjectHolder obj = object_.Execute(closure, context);
    auto class_inst_ptr_ = obj.TryAs<runtime::ClassInstance>();
    if (class_inst_ptr_) {
        class_inst_ptr_->Fields()[field_name_] = rv_.get()->Execute(closure, context);
    }
    else {
        //may be throw exeption?
    }
    return class_inst_ptr_->Fields().at(field_name_);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
    std::unique_ptr<Statement> else_body) 
    :condition_(std::move(condition))
    ,if_body_(std::move(if_body))
    , else_body_(std::move(else_body))
{
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    if (runtime::IsTrue(condition_.get()->Execute(closure, context))) {
        return if_body_.get()->Execute(closure, context);
    }
    else {
        if (else_body_) {
            return else_body_.get()->Execute(closure, context);
        }
        else {
            return runtime::ObjectHolder::None();//probably
        }
    }
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    if (runtime::IsTrue(left_obj)) {
        return ObjectHolder::Own(runtime::Bool(true));
    }
    else {
        ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
        if (runtime::IsTrue(right_obj)) {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        else {
            return ObjectHolder::Own(runtime::Bool(false));
        }

    }

}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    ObjectHolder left_obj = lhs_.get()->Execute(closure, context);
    if (!runtime::IsTrue(left_obj)) {
        return ObjectHolder::Own(runtime::Bool(false));
    }
    else {
        ObjectHolder right_obj = rhs_.get()->Execute(closure, context);
        if (!runtime::IsTrue(right_obj)) {
            return ObjectHolder::Own(runtime::Bool(false));
        }
        else {
            return ObjectHolder::Own(runtime::Bool(true));
        }

    }
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = argument_.get()->Execute(closure, context);
    if (runtime::IsTrue(obj)) {
        return ObjectHolder::Own(runtime::Bool(false));
    }
    else {
        return ObjectHolder::Own(runtime::Bool(true));
    }
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)) 
    , comp_(std::move(cmp))
{
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    const runtime::ObjectHolder left = lhs_.get()->Execute(closure, context);
    const runtime::ObjectHolder right = rhs_.get()->Execute(closure, context);
    return runtime::ObjectHolder::Own(runtime::Bool(comp_(left, right, context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) 
    :class_p_{ class_ }
    ,args_(std::move(args))
{
}

NewInstance::NewInstance(const runtime::Class& class_) 
    :class_p_(class_)
{
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (class_p_.HasMethod(INIT_METHOD, args_.size())) {
        std::vector<runtime::ObjectHolder> args_object(args_.size());
        for (size_t i = 0; i < args_.size(); ++i) {
            args_object[i] = args_[i].get()->Execute(closure, context);
        }
        class_p_.Call(INIT_METHOD, args_object, context);
    }
    return ObjectHolder::Share(class_p_);
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
    :body_(std::move(body))
{
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_.get()->Execute(closure, context);
        return runtime::ObjectHolder::None();
    }
    catch (runtime::ObjectHolder& object) {
        return object;
    }
}

}  // namespace ast