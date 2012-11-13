//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// IR management objects for type metadata and other tracking
//

#include "pch.h"

#include "Compiler/Intermediate Representations/Semantic Validation/TypeSpace.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Namespace.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Structure.h"

#include "Utility/StringPool.h"

#include "Compiler/Exceptions.h"
#include "Compiler/CompileErrors.h"
#include "Compiler/Session.h"


using namespace IRSemantics;


GlobalIDSpace::GlobalIDSpace()
	: CounterStructureTypeIDs(0),
	  CounterUnitTypeIDs(0),
	  CounterSumTypeIDs(0),
	  CounterTemplateInstantiations(0)
{
}

VM::EpochTypeID GlobalIDSpace::NewTemplateInstantiation()
{
	return (CounterTemplateInstantiations++) | VM::EpochTypeFamily_TemplateInstance;
}

VM::EpochTypeID GlobalIDSpace::NewStructureTypeID()
{
	return (CounterStructureTypeIDs++) | VM::EpochTypeFamily_Structure;
}

VM::EpochTypeID GlobalIDSpace::NewUnitTypeID()
{
	return (CounterUnitTypeIDs++) | VM::EpochTypeFamily_Unit;
}

VM::EpochTypeID GlobalIDSpace::NewSumTypeID()
{
	return (CounterSumTypeIDs++) | VM::EpochTypeFamily_SumType;
}


StructureTable::StructureTable(TypeSpace& typespace)
	: MyTypeSpace(typespace)
{
}

StructureTable::~StructureTable()
{
	for(std::map<StringHandle, Structure*>::iterator iter = NameToDefinitionMap.begin(); iter != NameToDefinitionMap.end(); ++iter)
		delete iter->second;
}


VM::EpochTypeID StructureTable::GetMemberType(StringHandle structurename, StringHandle membername) const
{
	if(MyTypeSpace.Templates.NameToTypeMap.find(structurename) != MyTypeSpace.Templates.NameToTypeMap.end())
		structurename = MyTypeSpace.Templates.GetTemplateForInstance(structurename);

	std::map<StringHandle, Structure*>::const_iterator defiter = NameToDefinitionMap.find(structurename);
	if(defiter == NameToDefinitionMap.end())
	{
		if(!MyTypeSpace.MyNamespace.Parent)
			throw InternalException("Invalid structure name");

		return MyTypeSpace.MyNamespace.Parent->Types.Structures.GetMemberType(structurename, membername);
	}

	const std::vector<std::pair<StringHandle, StructureMember*> >& members = defiter->second->GetMembers();
	for(std::vector<std::pair<StringHandle, StructureMember*> >::const_iterator iter = members.begin(); iter != members.end(); ++iter)
	{
		if(iter->first == membername)
			return iter->second->GetEpochType(MyTypeSpace.MyNamespace);
	}

	throw InternalException("Invalid structure member");
}

StringHandle StructureTable::GetConstructorName(StringHandle structurename) const
{
	return NameToDefinitionMap.find(structurename)->second->GetConstructorName();
}

void StructureTable::Add(StringHandle name, Structure* structure, CompileErrors& errors)
{
	if(NameToDefinitionMap.find(name) != NameToDefinitionMap.end())
	{
		delete structure;
		errors.SemanticError("Duplicate structure name");
		return;
	}

	NameToDefinitionMap.insert(std::make_pair(name, structure));
	NameToTypeMap.insert(std::make_pair(name, MyTypeSpace.IDSpace.NewStructureTypeID()));
	
	MyTypeSpace.MyNamespace.Functions.GenerateStructureFunctions(name, structure);
}


SumTypeTable::SumTypeTable(TypeSpace& typespace)
	: MyTypeSpace(typespace)
{
}


VM::EpochTypeID SumTypeTable::Add(const std::wstring& name, CompileErrors& errors)
{
	StringHandle namehandle = MyTypeSpace.MyNamespace.Strings.Pool(name);

	if(MyTypeSpace.GetTypeByName(namehandle) != VM::EpochType_Error)
		errors.SemanticError("Type name already in use");

	VM::EpochTypeID type = MyTypeSpace.IDSpace.NewSumTypeID();
	NameToTypeMap[namehandle] = type;

	return type;
}



bool SumTypeTable::IsBaseType(VM::EpochTypeID sumtypeid, VM::EpochTypeID basetype) const
{
	std::map<VM::EpochTypeID, std::set<StringHandle> >::const_iterator iter = BaseTypeNames.find(sumtypeid);
	if(iter == BaseTypeNames.end())
	{
		if(MyTypeSpace.MyNamespace.Parent)
			return MyTypeSpace.MyNamespace.Parent->Types.SumTypes.IsBaseType(sumtypeid, basetype);

		throw InternalException("Undefined sum type");
	}

	for(std::set<StringHandle>::const_iterator btiter = iter->second.begin(); btiter != iter->second.end(); ++btiter)
	{
		if(MyTypeSpace.GetTypeByName(*btiter) == basetype)
			return true;
	}

	return false;
}

void SumTypeTable::AddBaseTypeToSumType(VM::EpochTypeID sumtypeid, StringHandle basetypename)
{
	BaseTypeNames[sumtypeid].insert(basetypename);
}

StringHandle SumTypeTable::InstantiateTemplate(StringHandle templatename, const CompileTimeParameterVector& originalargs)
{
	CompileTimeParameterVector args(originalargs);
	for(CompileTimeParameterVector::iterator iter = args.begin(); iter != args.end(); ++iter)
	{
		// TODO - filter this to arguments that are typenames?
		if(MyTypeSpace.Aliases.HasWeakAliasNamed(iter->Payload.LiteralStringHandleValue))
			iter->Payload.LiteralStringHandleValue = MyTypeSpace.Aliases.GetWeakTypeBaseName(iter->Payload.LiteralStringHandleValue);
	}

	TypeSpace* typespace = &MyTypeSpace;
	while(typespace)
	{
		//if(typespace->SumTypes.NameToTypeMap.find(templatename) != typespace->SumTypes.NameToTypeMap.end())
		//	return templatename;

		InstantiationMap::iterator iter = typespace->SumTypes.Instantiations.find(templatename);
		if(iter != typespace->SumTypes.Instantiations.end())
		{
			InstancesAndArguments& instances = iter->second;

			for(InstancesAndArguments::const_iterator instanceiter = instances.begin(); instanceiter != instances.end(); ++instanceiter)
			{
				if(args.size() != instanceiter->second.size())
					continue;

				bool match = true;
				for(unsigned i = 0; i < args.size(); ++i)
				{
					if(!args[i].PatternMatch(instanceiter->second[i]))
					{
						match = false;
						break;
					}
				}

				if(match)
					return instanceiter->first;
			}
		}

		if(!typespace->MyNamespace.Parent)
			break;

		typespace = &typespace->MyNamespace.Parent->Types;
	}

	VM::EpochTypeID type = MyTypeSpace.IDSpace.NewSumTypeID();
	std::wstring mangled = GenerateTemplateMangledName(type);
	StringHandle mangledname = MyTypeSpace.MyNamespace.Strings.Pool(mangled);
	Instantiations[templatename].insert(std::make_pair(mangledname, args));
	NameToTypeMap[mangledname] = type;
	
	for(std::set<StringHandle>::const_iterator iter = BaseTypeNames[NameToTypeMap[templatename]].begin(); iter != BaseTypeNames[NameToTypeMap[templatename]].end(); ++iter)
	{
		if(MyTypeSpace.Structures.IsStructureTemplate(*iter))
		{
			// TODO - remap passed-in arguments to the arguments expected by this structure template
			StringHandle structurename = MyTypeSpace.Templates.InstantiateStructure(*iter, args);
			BaseTypeNames[type].insert(structurename);
		}
		else
			BaseTypeNames[type].insert(*iter);
	}

	return mangledname;
}

std::wstring SumTypeTable::GenerateTemplateMangledName(VM::EpochTypeID type)
{
	std::wostringstream formatter;
	formatter << "@@sumtemplateinst@" << (type & 0xffffff);
	return formatter.str();
}

bool SumTypeTable::IsTemplate(StringHandle name) const
{
	if(NameToParamsMap.find(name) != NameToParamsMap.end())
		return true;

	if(MyTypeSpace.MyNamespace.Parent)
		return MyTypeSpace.MyNamespace.Parent->Types.SumTypes.IsTemplate(name);

	return false;
}

void SumTypeTable::AddTemplateParameter(VM::EpochTypeID sumtype, StringHandle name)
{
	NameToParamsMap[MyTypeSpace.GetNameOfType(sumtype)].push_back(name);
}



TypeAliasTable::TypeAliasTable(TypeSpace& typespace)
	: MyTypeSpace(typespace)
{
}

VM::EpochTypeID TypeAliasTable::GetStrongRepresentation(VM::EpochTypeID aliastypeid) const
{
	std::map<VM::EpochTypeID, VM::EpochTypeID>::const_iterator iter = StrongRepresentationTypes.find(aliastypeid);
	if(iter == StrongRepresentationTypes.end())
		throw InternalException("Invalid strong type alias");

	return iter->second;
}

VM::EpochTypeID TypeAliasTable::GetStrongRepresentationName(VM::EpochTypeID aliastypeid) const
{
	std::map<VM::EpochTypeID, StringHandle>::const_iterator iter = StrongRepresentationNames.find(aliastypeid);
	if(iter == StrongRepresentationNames.end())
		throw InternalException("Invalid strong type alias");

	return iter->second;
}

void TypeAliasTable::AddWeakAlias(StringHandle aliasname, VM::EpochTypeID representationtype)
{
	WeakNameToTypeMap[aliasname] = representationtype;
}

bool TypeAliasTable::HasWeakAliasNamed(StringHandle name) const
{
	return WeakNameToTypeMap.find(name) != WeakNameToTypeMap.end();
}


StringHandle TypeAliasTable::GetWeakTypeBaseName(StringHandle name) const
{
	return MyTypeSpace.GetNameOfType(WeakNameToTypeMap.find(name)->second);
}


void TypeAliasTable::AddStrongAlias(StringHandle aliasname, VM::EpochTypeID representationtype, StringHandle representationname)
{
	VM::EpochTypeID newtypeid = MyTypeSpace.IDSpace.NewUnitTypeID();
	StrongNameToTypeMap[aliasname] = newtypeid;
	StrongRepresentationTypes[newtypeid] = representationtype;
	StrongRepresentationNames[newtypeid] = representationname;
}

StringHandle SumTypeTable::MapConstructorName(StringHandle sumtypeoverloadname) const
{
	std::map<StringHandle, StringHandle>::const_iterator iter = NameToConstructorMap.find(sumtypeoverloadname);
	if(iter == NameToConstructorMap.end())
	{
		if(MyTypeSpace.MyNamespace.Parent)
			return MyTypeSpace.MyNamespace.Parent->Types.SumTypes.MapConstructorName(sumtypeoverloadname);

		return sumtypeoverloadname;
	}

	return iter->second;
}

unsigned SumTypeTable::GetNumBaseTypes(VM::EpochTypeID type) const
{
	std::map<VM::EpochTypeID, std::set<StringHandle> >::const_iterator iter = BaseTypeNames.find(type);
	if(iter == BaseTypeNames.end())
	{
		if(MyTypeSpace.MyNamespace.Parent)
			return MyTypeSpace.MyNamespace.Parent->Types.SumTypes.GetNumBaseTypes(type);

		throw InternalException("Not a defined sum type");
	}

	return iter->second.size();
}

std::map<VM::EpochTypeID, std::set<VM::EpochTypeID> > SumTypeTable::GetDefinitions() const
{
	std::map<VM::EpochTypeID, std::set<VM::EpochTypeID> > ret;
	for(std::map<VM::EpochTypeID, std::set<StringHandle> >::const_iterator typeiter = BaseTypeNames.begin(); typeiter != BaseTypeNames.end(); ++typeiter)
	{
		for(std::set<StringHandle>::const_iterator nameiter = typeiter->second.begin(); nameiter != typeiter->second.end(); ++nameiter)
			ret[typeiter->first].insert(MyTypeSpace.GetTypeByName(*nameiter));
	}

	return ret;
}


TemplateTable::TemplateTable(TypeSpace& typespace)
	: MyTypeSpace(typespace)
{
}

std::wstring TemplateTable::GenerateTemplateMangledName(VM::EpochTypeID type)
{
	std::wostringstream formatter;
	formatter << "@@templateinst@" << (type & 0xffffff);
	return formatter.str();
}


StringHandle TemplateTable::InstantiateStructure(StringHandle templatename, const CompileTimeParameterVector& originalargs)
{
	CompileTimeParameterVector args(originalargs);
	for(CompileTimeParameterVector::iterator iter = args.begin(); iter != args.end(); ++iter)
	{
		// TODO - filter this to arguments that are typenames?
		if(MyTypeSpace.Aliases.HasWeakAliasNamed(iter->Payload.LiteralStringHandleValue))
			iter->Payload.LiteralStringHandleValue = MyTypeSpace.Aliases.GetWeakTypeBaseName(iter->Payload.LiteralStringHandleValue);
	}

	TypeSpace* typespace = &MyTypeSpace;
	while(typespace)
	{
		if(typespace->Templates.NameToTypeMap.find(templatename) != typespace->Templates.NameToTypeMap.end())
			return templatename;

		InstantiationMap::iterator iter = typespace->Templates.Instantiations.find(templatename);
		if(iter != typespace->Templates.Instantiations.end())
		{
			InstancesAndArguments& instances = iter->second;

			for(InstancesAndArguments::const_iterator instanceiter = instances.begin(); instanceiter != instances.end(); ++instanceiter)
			{
				if(args.size() != instanceiter->second.size())
					continue;

				bool match = true;
				for(unsigned i = 0; i < args.size(); ++i)
				{
					if(!args[i].PatternMatch(instanceiter->second[i]))
					{
						match = false;
						break;
					}
				}

				if(match)
					return instanceiter->first;
			}
		}

		if(!typespace->MyNamespace.Parent)
			break;

		typespace = &typespace->MyNamespace.Parent->Types;
	}

	VM::EpochTypeID type = typespace->IDSpace.NewTemplateInstantiation();
	std::wstring mangled = GenerateTemplateMangledName(type);
	StringHandle mangledname = typespace->MyNamespace.Strings.Pool(mangled);
	typespace->Templates.Instantiations[templatename].insert(std::make_pair(mangledname, args));
	typespace->Templates.NameToTypeMap[mangledname] = type;

	StringHandle constructorname = MyTypeSpace.MyNamespace.Strings.Pool(mangled + L"@@constructor");
	StringHandle anonconstructorname = MyTypeSpace.MyNamespace.Strings.Pool(mangled + L"@@anonconstructor");

	typespace->Templates.ConstructorNameCache.Add(mangledname, constructorname);
	typespace->Templates.AnonConstructorNameCache.Add(mangledname, anonconstructorname);

	return mangledname;
}

StringHandle TemplateTable::FindConstructorName(StringHandle instancename) const
{
	StringHandle ret = ConstructorNameCache.Find(instancename);
	if(!ret && MyTypeSpace.MyNamespace.Parent)
		return MyTypeSpace.MyNamespace.Parent->Types.Templates.FindConstructorName(instancename);

	return ret;
}

StringHandle TemplateTable::FindAnonConstructorName(StringHandle instancename) const
{
	StringHandle ret = AnonConstructorNameCache.Find(instancename);
	if(!ret && MyTypeSpace.MyNamespace.Parent)
		return MyTypeSpace.MyNamespace.Parent->Types.Templates.FindAnonConstructorName(instancename);

	return ret;
}

StringHandle TemplateTable::GetTemplateForInstance(StringHandle instancename) const
{
	for(InstantiationMap::const_iterator iter = Instantiations.begin(); iter != Instantiations.end(); ++iter)
	{
		if(iter->second.find(instancename) != iter->second.end())
			return iter->first;
	}

	throw InternalException("Invalid template instance");
}

bool StructureTable::IsStructureTemplate(StringHandle name) const
{
	std::map<StringHandle, Structure*>::const_iterator iter = NameToDefinitionMap.find(name);
	if(iter == NameToDefinitionMap.end())
	{
		if(MyTypeSpace.MyNamespace.Parent)
			return MyTypeSpace.MyNamespace.Parent->Types.Structures.IsStructureTemplate(name);

		return false;
	}

	return iter->second->IsTemplate();
}


#pragma warning(push)
#pragma warning(disable: 4355)

TypeSpace::TypeSpace(Namespace& mynamespace, GlobalIDSpace& idspace)
	: MyNamespace(mynamespace),
	  IDSpace(idspace),
	  Aliases(*this),
	  Structures(*this),
	  SumTypes(*this),
	  Templates(*this)
{
}

#pragma warning(pop)

VM::EpochTypeID TypeSpace::GetTypeByName(StringHandle name) const
{
	// TODO - replace this with a less hard-coded solution
	const std::wstring& type = MyNamespace.Strings.GetPooledString(name);

	if(type == L"integer")
		return VM::EpochType_Integer;
	else if(type == L"integer16")
		return VM::EpochType_Integer16;
	else if(type == L"string")
		return VM::EpochType_String;
	else if(type == L"boolean")
		return VM::EpochType_Boolean;
	else if(type == L"real")
		return VM::EpochType_Real;
	else if(type == L"buffer")
		return VM::EpochType_Buffer;
	else if(type == L"identifier")
		return VM::EpochType_Identifier;
	else if(type == L"nothing")
		return VM::EpochType_Nothing;

	{
		std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Structures.NameToTypeMap.find(name);
		if(iter != Structures.NameToTypeMap.end())
			return iter->second;
	}
	
	{
		std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Aliases.WeakNameToTypeMap.find(name);
		if(iter != Aliases.WeakNameToTypeMap.end())
			return iter->second;
	}

	{
		std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Aliases.StrongNameToTypeMap.find(name);
		if(iter != Aliases.StrongNameToTypeMap.end())
			return iter->second;
	}

	{
		std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = SumTypes.NameToTypeMap.find(name);
		if(iter != SumTypes.NameToTypeMap.end())
		{
			if(!SumTypes.IsTemplate(name))
				return iter->second;
		}
	}

	{
		std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Templates.NameToTypeMap.find(name);
		if(iter != Templates.NameToTypeMap.end())
			return iter->second;
	}

	{
		std::map<StringHandle, StringHandle>::const_iterator iter = SumTypes.NameToConstructorMap.find(name);
		if(iter != SumTypes.NameToConstructorMap.end())
			return GetTypeByName(iter->second);
	}

	if(MyNamespace.Parent)
		return MyNamespace.Parent->Types.GetTypeByName(name);

	return VM::EpochType_Error;
}

StringHandle TypeSpace::GetNameOfType(VM::EpochTypeID type) const
{
	switch(type)
	{
	case VM::EpochType_Boolean:
		return MyNamespace.Strings.Find(L"boolean");

	case VM::EpochType_Buffer:
		return MyNamespace.Strings.Find(L"buffer");

	case VM::EpochType_Identifier:
		return MyNamespace.Strings.Find(L"identifier");

	case VM::EpochType_Integer:
		return MyNamespace.Strings.Find(L"integer");

	case VM::EpochType_Integer16:
		return MyNamespace.Strings.Find(L"integer16");

	case VM::EpochType_Nothing:
		return MyNamespace.Strings.Find(L"nothing");

	case VM::EpochType_Real:
		return MyNamespace.Strings.Find(L"real");

	case VM::EpochType_String:
		return MyNamespace.Strings.Find(L"string");

	case VM::EpochType_Function:
		return MyNamespace.Strings.Pool(L"function");
	}

	// TODO - extend to other names

	for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Structures.NameToTypeMap.begin(); iter != Structures.NameToTypeMap.end(); ++iter)
	{
		if(iter->second == type)
			return iter->first;
	}

	for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Aliases.WeakNameToTypeMap.begin(); iter != Aliases.WeakNameToTypeMap.end(); ++iter)
	{
		if(iter->second == type)
			return iter->first;
	}

	for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Aliases.StrongNameToTypeMap.begin(); iter != Aliases.StrongNameToTypeMap.end(); ++iter)
	{
		if(iter->second == type)
			return iter->first;
	}

	for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = SumTypes.NameToTypeMap.begin(); iter != SumTypes.NameToTypeMap.end(); ++iter)
	{
		if(iter->second == type)
			return iter->first;
	}

	for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = Templates.NameToTypeMap.begin(); iter != Templates.NameToTypeMap.end(); ++iter)
	{
		if(iter->second == type)
			return iter->first;
	}

	if(MyNamespace.Parent)
		return MyNamespace.Parent->Types.GetNameOfType(type);

	//
	// This catches a potential logic bug in the compiler implementation.
	//
	// If a structure has been allocated a type ID, that type ID should never
	// be able to exist without being also bound to an identifier, even if that
	// identifier is just an automatically generated magic anonymous token.
	//
	// Callers cannot be faulted directly for this exception; the problem most
	// likely lies elsewhere in the structure type handling code.
	//
	throw InternalException("Type ID is not bound to any known identifier");

}

bool TypeSpace::Validate(CompileErrors& errors) const
{
	bool valid = true;

	for(std::map<VM::EpochTypeID, std::set<StringHandle> >::const_iterator iter = SumTypes.BaseTypeNames.begin(); iter != SumTypes.BaseTypeNames.end(); ++iter)
	{
		for(std::set<StringHandle>::const_iterator setiter = iter->second.begin(); setiter != iter->second.end(); ++setiter)
		{
			if(GetTypeByName(*setiter) == VM::EpochType_Error)
			{
				// TODO - set context
				errors.SemanticError("Base type not defined");
				valid = false;
			}
		}
	}

	for(std::map<StringHandle, Structure*>::const_iterator iter = Structures.NameToDefinitionMap.begin(); iter != Structures.NameToDefinitionMap.end(); ++iter)
	{
		if(!iter->second->Validate(MyNamespace, errors))
			valid = false;
	}

	return valid;
}

bool TypeSpace::CompileTimeCodeExecution(CompileErrors& errors)
{
	for(std::map<StringHandle, Structure*>::iterator iter = Structures.NameToDefinitionMap.begin(); iter != Structures.NameToDefinitionMap.end(); ++iter)
	{
		if(!iter->second->CompileTimeCodeExecution(iter->first, MyNamespace, errors))
			return false;
	}

	for(std::map<StringHandle, Structure*>::iterator iter = Structures.NameToDefinitionMap.begin(); iter != Structures.NameToDefinitionMap.end(); ++iter)
	{
		if(iter->second->IsTemplate())
		{
			const InstancesAndArguments& instances = Templates.Instantiations[iter->first];
			if(instances.empty())
			{
				errors.SemanticError("Template never instantiated");
				return false;
			}

			for(InstancesAndArguments::const_iterator instanceiter = instances.begin(); instanceiter != instances.end(); ++instanceiter)
			{
				if(!iter->second->InstantiateTemplate(instanceiter->first, instanceiter->second, MyNamespace, errors))
					return false;
			}
		}
	}


	for(std::map<VM::EpochTypeID, std::set<StringHandle> >::const_iterator iter = SumTypes.BaseTypeNames.begin(); iter != SumTypes.BaseTypeNames.end(); ++iter)
	{
		VM::EpochTypeID sumtypeid = iter->first;
		if(SumTypes.IsTemplate(GetNameOfType(sumtypeid)))
			continue;

		// TODO - refactor a bit
		//////////////////////////////////////
		std::wostringstream overloadnamebuilder;
		overloadnamebuilder << L"@@sumtypeconstructor@" << sumtypeid << L"@" << sumtypeid;
		StringHandle sumtypeconstructorname = 0;
		for(std::map<StringHandle, VM::EpochTypeID>::const_iterator niter = SumTypes.NameToTypeMap.begin(); niter != SumTypes.NameToTypeMap.end(); ++niter)
		{
			if(niter->second == sumtypeid)
			{
				sumtypeconstructorname = niter->first;
				break;
			}
		}

		if(!sumtypeconstructorname)
			throw InternalException("Missing sum type name mapping");

		StringHandle overloadname = MyNamespace.Strings.Pool(overloadnamebuilder.str());
		MyNamespace.Session.FunctionOverloadNames[sumtypeconstructorname].insert(overloadname);

		FunctionSignature signature;
		signature.AddParameter(L"@id", VM::EpochType_Identifier, false);
		signature.AddParameter(L"@value", sumtypeid, false);

		// TODO - evil hackery here
		MyNamespace.Session.CompileTimeHelpers.insert(std::make_pair(sumtypeconstructorname, MyNamespace.Session.CompileTimeHelpers.find(MyNamespace.Strings.Find(L"integer"))->second));
		MyNamespace.Session.FunctionSignatures.insert(std::make_pair(overloadname, signature));
		SumTypes.NameToConstructorMap[overloadname] = sumtypeconstructorname;
		//////////////////////////////////////

		for(std::set<StringHandle>::const_iterator btiter = iter->second.begin(); btiter != iter->second.end(); ++btiter)
		{
			StringHandle basetypename = *btiter;

			std::wostringstream overloadnamebuilder;
			overloadnamebuilder << L"@@sumtypeconstructor@" << sumtypeid << L"@" << MyNamespace.Strings.GetPooledString(basetypename);
			StringHandle sumtypeconstructorname = 0;
			for(std::map<StringHandle, VM::EpochTypeID>::const_iterator iter = SumTypes.NameToTypeMap.begin(); iter != SumTypes.NameToTypeMap.end(); ++iter)
			{
				if(iter->second == sumtypeid)
				{
					sumtypeconstructorname = iter->first;
					break;
				}
			}

			if(!sumtypeconstructorname)
				throw InternalException("Missing sum type name mapping");

			StringHandle overloadname = MyNamespace.Strings.Pool(overloadnamebuilder.str());
			MyNamespace.Session.FunctionOverloadNames[sumtypeconstructorname].insert(overloadname);

			VM::EpochTypeFamily family = VM::GetTypeFamily(GetTypeByName(basetypename));
			switch(family)
			{
			case VM::EpochTypeFamily_Magic:
			case VM::EpochTypeFamily_Primitive:
			case VM::EpochTypeFamily_GC:
				// No adjustment needed
				break;

			case VM::EpochTypeFamily_Structure:
				basetypename = Structures.GetConstructorName(basetypename);
				break;

			case VM::EpochTypeFamily_TemplateInstance:
				basetypename = Templates.FindConstructorName(basetypename);
				break;

			default:
				throw NotImplementedException("Sum types with this base type not supported");
			}

			MyNamespace.Session.CompileTimeHelpers.insert(std::make_pair(overloadname, MyNamespace.Session.CompileTimeHelpers.find(basetypename)->second));
			MyNamespace.Session.FunctionSignatures.insert(std::make_pair(overloadname, MyNamespace.Session.FunctionSignatures.find(basetypename)->second));
			SumTypes.NameToConstructorMap[overloadname] = basetypename;
		}
	}

	return true;
}

