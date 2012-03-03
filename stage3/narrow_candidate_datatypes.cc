/*
 *  matiec - a compiler for the programming languages defined in IEC 61131-3
 *
 *  Copyright (C) 2009-2012  Mario de Sousa (msousa@fe.up.pt)
 *  Copyright (C) 2012       Manuele Conti (manuele.conti@sirius-es.it)
 *  Copyright (C) 2012       Matteo Facchinetti (matteo.facchinetti@sirius-es.it)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This code is made available on the understanding that it will not be
 * used in safety-critical situations without a full and competent review.
 */

/*
 * An IEC 61131-3 compiler.
 *
 * Based on the
 * FINAL DRAFT - IEC 61131-3, 2nd Ed. (2001-12-10)
 *
 */


/*
 *  Narrow class select and store a data type from candidate data types list for all symbols
 */

#include "narrow_candidate_datatypes.hh"
#include "datatype_functions.hh"
#include <typeinfo>
#include <list>
#include <string>
#include <string.h>
#include <strings.h>


/* set to 1 to see debug info during execution */
static int debug = 0;

narrow_candidate_datatypes_c::narrow_candidate_datatypes_c(symbol_c *ignore) {
}

narrow_candidate_datatypes_c::~narrow_candidate_datatypes_c(void) {
}


/* Only set the symbol's desired datatype to 'datatype' if that datatype is in the candidate_datatype list */
static void set_datatype(symbol_c *datatype, symbol_c *symbol) {
  
	/* If we are trying to set to the undefined type, and the symbol's datatype has already been set to something else, 
	 * we abort the compoiler as I don't think this should ever occur. 
	 * However, I (Mario) am not too sure of this, so if the compiler ever aborts here, please analyse the situation
	 * carefully as it might be perfectly legal and something I have missed!
	 */
	if ((NULL == datatype) && (NULL != symbol->datatype)) ERROR;
	
	if ((NULL == datatype) && (NULL == symbol->datatype)) return;
	
	if (search_in_candidate_datatype_list(datatype, symbol->candidate_datatypes) < 0)
		symbol->datatype = &(search_constant_type_c::invalid_type_name);   
	else {
		if (NULL == symbol->datatype)   
			/* not yet set to anything, so we set it to the requested data type */
			symbol->datatype = datatype; 
		else {
			/* had already been set previously to some data type. Let's check if they are the same! */
			if (!is_type_equal(symbol->datatype, datatype))
				symbol->datatype = &(search_constant_type_c::invalid_type_name);
// 			else 
				/* we leave it unchanged, as it is the same as the requested data type! */
		}
	}
}



/* Only set the symbol's desired datatype to 'datatype' if that datatype is in the candidate_datatype list */
// static void set_datatype_in_prev_il_instructions(symbol_c *datatype, std::vector <symbol_c *> prev_il_instructions) {
static void set_datatype_in_prev_il_instructions(symbol_c *datatype, il_instruction_c *symbol) {
	if (NULL == symbol) ERROR;
	for (unsigned int i = 0; i < symbol->prev_il_instruction.size(); i++)
		set_datatype(datatype, symbol->prev_il_instruction[i]);
}



bool narrow_candidate_datatypes_c::is_widening_compatible(symbol_c *left_type, symbol_c *right_type, symbol_c *result_type, const struct widen_entry widen_table[]) {
	for (int k = 0; NULL != widen_table[k].left;  k++) {
		if        ((typeid(*left_type)   == typeid(*widen_table[k].left))
		        && (typeid(*right_type)  == typeid(*widen_table[k].right))
				&& (typeid(*result_type) == typeid(*widen_table[k].result))) {
			return true;
		}
	}
	return false;
}

/*
 * All parameters being passed to the called function MUST be in the parameter list to which f_call points to!
 * This means that, for non formal function calls in IL, de current (default value) must be artificially added to the
 * beginning of the parameter list BEFORE calling handle_function_call().
 */
void narrow_candidate_datatypes_c::narrow_nonformal_call(symbol_c *f_call, symbol_c *f_decl, int *ext_parm_count) {
	symbol_c *call_param_value,  *param_type;
	identifier_c *param_name;
	function_param_iterator_c       fp_iterator(f_decl);
	function_call_param_iterator_c fcp_iterator(f_call);
	int extensible_parameter_highest_index = -1;
	unsigned int i;

	if (NULL != ext_parm_count) *ext_parm_count = -1;

	/* Iterating through the non-formal parameters of the function call */
	while((call_param_value = fcp_iterator.next_nf()) != NULL) {
		/* Obtaining the type of the value being passed in the function call */
		/* Iterate to the next parameter of the function being called.
		 * Get the name of that parameter, and ignore if EN or ENO.
		 */
		do {
			param_name = fp_iterator.next();
			/* If there is no other parameter declared, then we are passing too many parameters... */
			/* This error should have been caught in fill_candidate_datatypes_c, but may occur here again when we handle FB invocations! 
			 * In this case, we carry on analysing the code in order to be able to provide relevant error messages
			 * for that code too!
			 */
			if(param_name == NULL) break;
		} while ((strcmp(param_name->value, "EN") == 0) || (strcmp(param_name->value, "ENO") == 0));

		/* Set the desired datatype for this parameter, and call it recursively. */
		/* Note that if the call has more parameters than those declared in the function/FB declaration,
		 * we may be setting this to NULL!
		 */
		symbol_c *desired_datatype = base_type(fp_iterator.param_type());
		if ((NULL != param_name) && (NULL == desired_datatype)) ERROR;
		if ((NULL == param_name) && (NULL != desired_datatype)) ERROR;

		/* NOTE: When we are handling a nonformal function call made from IL, the first parameter is the 'default' or 'current'
		 *       il value. However, a pointer to a copy of the prev_il_instruction is pre-pended into the operand list, so 
		 *       the call 
		 *       call_param_value->accept(*this);
		 *       may actually be calling an object of the base symbol_c .
		 */
		set_datatype(desired_datatype, call_param_value);
		call_param_value->accept(*this);

		if (NULL != param_name) 
			if (extensible_parameter_highest_index < fp_iterator.extensible_param_index())
				extensible_parameter_highest_index = fp_iterator.extensible_param_index();
	}
	/* In the case of a call to an extensible function, we store the highest index 
	 * of the extensible parameters this particular call uses, in the symbol_c object
	 * of the function call itself!
	 * In calls to non-extensible functions, this value will be set to -1.
	 * This information is later used in stage4 to correctly generate the
	 * output code.
	 */
	if ((NULL != ext_parm_count) && (extensible_parameter_highest_index >=0) /* if call to extensible function */)
		*ext_parm_count = 1 + extensible_parameter_highest_index - fp_iterator.first_extensible_param_index();
}



void narrow_candidate_datatypes_c::narrow_formal_call(symbol_c *f_call, symbol_c *f_decl, int *ext_parm_count) {
	symbol_c *call_param_value, *call_param_name, *param_type;
	symbol_c *verify_duplicate_param;
	identifier_c *param_name;
	function_param_iterator_c       fp_iterator(f_decl);
	function_call_param_iterator_c fcp_iterator(f_call);
	int extensible_parameter_highest_index = -1;
	identifier_c *extensible_parameter_name;
	unsigned int i;

	if (NULL != ext_parm_count) *ext_parm_count = -1;
	/* Iterating through the formal parameters of the function call */
	while((call_param_name = fcp_iterator.next_f()) != NULL) {

		/* Obtaining the value being passed in the function call */
		call_param_value = fcp_iterator.get_current_value();
		/* the following should never occur. If it does, then we have a bug in our code... */
		if (NULL == call_param_value) ERROR;

		/* Find the corresponding parameter in function declaration */
		param_name = fp_iterator.search(call_param_name);

		/* Set the desired datatype for this parameter, and call it recursively. */
		/* NOTE: When handling a FB call, this narrow_formal_call() may be called to analyse
		 *       an invalid FB call (call with parameters that do not exist on the FB declaration).
		 *       For this reason, the param_name may come out as NULL!
		 */
		symbol_c *desired_datatype = base_type(fp_iterator.param_type());
		if ((NULL != param_name) && (NULL == desired_datatype)) ERROR;
		if ((NULL == param_name) && (NULL != desired_datatype)) ERROR;

		/* set the desired data type for this parameter */
		set_datatype(desired_datatype, call_param_value);
		/* And recursively call that parameter/expression, so it can propagate that info */
		call_param_value->accept(*this);

		/* set the extensible_parameter_highest_index, which will be needed in stage 4 */
		/* This value says how many extensible parameters are being passed to the standard function */
		if (NULL != param_name) 
			if (extensible_parameter_highest_index < fp_iterator.extensible_param_index())
				extensible_parameter_highest_index = fp_iterator.extensible_param_index();
	}
	/* call is compatible! */

	/* In the case of a call to an extensible function, we store the highest index 
	 * of the extensible parameters this particular call uses, in the symbol_c object
	 * of the function call itself!
	 * In calls to non-extensible functions, this value will be set to -1.
	 * This information is later used in stage4 to correctly generate the
	 * output code.
	 */
	if ((NULL != ext_parm_count) && (extensible_parameter_highest_index >=0) /* if call to extensible function */)
		*ext_parm_count = 1 + extensible_parameter_highest_index - fp_iterator.first_extensible_param_index();
}


/*
typedef struct {
  symbol_c *function_name,
  symbol_c *nonformal_operand_list,
  symbol_c *   formal_operand_list,

  std::vector <symbol_c *> &candidate_functions,  
  symbol_c &*called_function_declaration,
  int      &extensible_param_count
} generic_function_call_t;
*/
void narrow_candidate_datatypes_c::narrow_function_invocation(symbol_c *fcall, generic_function_call_t fcall_data) {
	/* set the called_function_declaration. */
	fcall_data.called_function_declaration = NULL;

	/* set the called_function_declaration taking into account the datatype that we need to return */
	for(unsigned int i = 0; i < fcall->candidate_datatypes.size(); i++) {
		if (is_type_equal(fcall->candidate_datatypes[i], fcall->datatype)) {
			fcall_data.called_function_declaration = fcall_data.candidate_functions[i];
			break;
		}
	}

	/* NOTE: If we can't figure out the declaration of the function being called, this is not 
	 *       necessarily an internal compiler error. It could be because the symbol->datatype is NULL
	 *       (because the ST code being analysed has an error _before_ this function invocation).
	 *       However, we don't just give, up, we carry on recursivly analysing the code, so as to be
	 *       able to print out any error messages related to the parameters being passed in this function 
	 *       invocation.
	 */
	/* if (NULL == symbol->called_function_declaration) ERROR; */
	if (fcall->candidate_datatypes.size() == 1) {
		/* If only one function declaration, then we use that (even if symbol->datatypes == NULL)
		 * so we can check for errors in the expressions used to pass parameters in this
		 * function invocation.
		 */
		fcall_data.called_function_declaration = fcall_data.candidate_functions[0];
	}

	/* If an overloaded function is being invoked, and we cannot determine which version to use,
	 * then we can not meaningfully verify the expressions used inside that function invocation.
	 * We simply give up!
	 */
	if (NULL == fcall_data.called_function_declaration)
		return;

	if (NULL != fcall_data.nonformal_operand_list)  narrow_nonformal_call(fcall, fcall_data.called_function_declaration, &(fcall_data.extensible_param_count));
	if (NULL != fcall_data.   formal_operand_list)     narrow_formal_call(fcall, fcall_data.called_function_declaration, &(fcall_data.extensible_param_count));

	return;
}




/* narrow implicit FB call in IL.
 * e.g.  CLK ton_var
 *        CU counter_var
 *
 * The algorithm will be to build a fake il_fb_call_c equivalent to the implicit IL FB call, and let 
 * the visit(il_fb_call_c *) method handle it!
 */
void *narrow_candidate_datatypes_c::narrow_implicit_il_fb_call(symbol_c *il_instruction, const char *param_name, symbol_c *&called_fb_declaration) {

	/* set the datatype of the il_operand, this is, the FB being called! */
	if (NULL != il_operand) {
		/* only set it if it is in the candidate datatypes list! */  
		set_datatype(called_fb_declaration, il_operand);
		il_operand->accept(*this);
	}
	symbol_c *fb_decl = il_operand->datatype;

	if (0 == fake_prev_il_instruction->prev_il_instruction.size()) {
		/* This IL implicit FB call (e.g. CLK ton_var) is not preceded by another IL instruction
		 * (or list of instructions) that will set the IL current/default value.
		 * We cannot proceed verifying type compatibility of something that does not exist.
		 */
		return NULL;
	}

	if (NULL == fb_decl) {
		/* the il_operand is a not FB instance */
		/* so we simply pass on the required datatype to the prev_il_instructions */
		/* The invalid FB invocation will be caught in the print_datatypes_error_c by analysing NULL value in il_operand->datatype! */
		set_datatype_in_prev_il_instructions(il_instruction->datatype, fake_prev_il_instruction);
		return NULL;
	}
	

	/* The value being passed to the 'param_name' parameter is actually the prev_il_instruction.
	 * However, we do not place that object directly in the fake il_param_list_c that we will be
	 * creating, since the visit(il_fb_call_c *) method will recursively call every object in that list.
	 * The il_prev_intruction object will be visited once we have handled this implici IL FB call
	 * (called from the instruction_list_c for() loop that works backwards). We DO NOT want to visit it twice.
	 * (Anyway, if we let the visit(il_fb_call_c *) recursively visit the current prev_il_instruction, this pointer
	 * would be changed to the IL instruction coming before the current prev_il_instruction! => things would get all messed up!)
	 * The easiest way to work around this is to simply use a new object, and copy the relevant details to that object!
	 */
	symbol_c param_value = *fake_prev_il_instruction; /* copy the candidate_datatypes list ! */

	identifier_c variable_name(param_name);
	// SYM_REF1(il_assign_operator_c, variable_name)
	il_assign_operator_c il_assign_operator(&variable_name);  
	// SYM_REF3(il_param_assignment_c, il_assign_operator, il_operand, simple_instr_list)
	il_param_assignment_c il_param_assignment(&il_assign_operator, &param_value/*il_operand*/, NULL);
	il_param_list_c il_param_list;
	il_param_list.add_element(&il_param_assignment);
	// SYM_REF4(il_fb_call_c, il_call_operator, fb_name, il_operand_list, il_param_list, symbol_c *called_fb_declaration)
	CAL_operator_c CAL_operator;
	il_fb_call_c il_fb_call(&CAL_operator, il_operand, NULL, &il_param_list);
	        
	/* A FB call does not return any datatype, but the IL instructions that come after this
	 * FB call may require a specific datatype in the il current/default variable, 
	 * so we must pass this information up to the IL instruction before the FB call, since it will
	 * be that IL instruction that will be required to produce the desired dtataype.
	 *
	 * The above will be done by the visit(il_fb_call_c *) method, so we must make sure to
	 * correctly set up the il_fb_call.datatype variable!
	 */
// 	copy_candidate_datatype_list(il_instruction/*from*/, &il_fb_call/*to*/);
	il_fb_call.called_fb_declaration = called_fb_declaration;
	il_fb_call.accept(*this);

	/* set the required datatype of the previous IL instruction! */
	/* NOTE:
	 * When handling these implicit IL calls, the parameter_value being passed to the FB parameter
	 * is actually the prev_il_instruction.
	 * 
	 * However, since the FB call does not change the value in the current/default IL variable, this value
	 * must also be used ny the IL instruction coming after this FB call.
	 *
	 * This means that we have two consumers/users for the same value. 
	 * We must therefore check whether the datatype required by the IL instructions following this FB call 
	 * is the same as that required for the first parameter. If not, then we have a semantic error,
	 * and we set it to invalid_type_name.
	 *
	 * However, we only do that if:
	 *  - The IL instruction that comes after this IL FB call actually asked this FB call for a specific 
	 *     datatype in the current/default vairable, once this IL FB call returns.
	 *     However, sometimes, (for e.g., this FB call is the last in the IL list) the subsequent FB to not aks this
	 *     FB call for any datatype. In that case, then the datatype required to pass to the first parameter of the
	 *     FB call must be left unchanged!
	 */
	if ((NULL == il_instruction->datatype) || (is_type_equal(param_value.datatype, il_instruction->datatype))) {
		set_datatype_in_prev_il_instructions(param_value.datatype, fake_prev_il_instruction);
	} else {
		set_datatype_in_prev_il_instructions(&search_constant_type_c::invalid_type_name, fake_prev_il_instruction);
	}
	return NULL;
}


/* a helper function... */
symbol_c *narrow_candidate_datatypes_c::base_type(symbol_c *symbol) {
	/* NOTE: symbol == NULL is valid. It will occur when, for e.g., an undefined/undeclared symbolic_variable is used
	 *       in the code.
	 */
	if (symbol == NULL) return NULL;
	return (symbol_c *)symbol->accept(search_base_type);	
}

/*********************/
/* B 1.2 - Constants */
/*********************/

/**********************/
/* B 1.3 - Data types */
/**********************/
/********************************/
/* B 1.3.3 - Derived data types */
/********************************/
/*  signed_integer DOTDOT signed_integer */
// SYM_REF2(subrange_c, lower_limit, upper_limit)
void *narrow_candidate_datatypes_c::visit(subrange_c *symbol) {
	symbol->lower_limit->datatype = symbol->datatype;
	symbol->lower_limit->accept(*this);
	symbol->upper_limit->datatype = symbol->datatype;
	symbol->upper_limit->accept(*this);
	return NULL;
}

/* simple_specification ASSIGN constant */
// SYM_REF2(simple_spec_init_c, simple_specification, constant)
void *narrow_candidate_datatypes_c::visit(simple_spec_init_c *symbol) {
	symbol_c *datatype = base_type(symbol->simple_specification); 
	if (NULL != symbol->constant) {
		int typeoffset = search_in_candidate_datatype_list(datatype, symbol->constant->candidate_datatypes);
		if (typeoffset >= 0)
			symbol->constant->datatype = symbol->constant->candidate_datatypes[typeoffset];
	}
	return NULL;
}

/*********************/
/* B 1.4 - Variables */
/*********************/

/********************************************/
/* B 1.4.1 - Directly Represented Variables */
/********************************************/

/*************************************/
/* B 1.4.2 - Multi-element variables */
/*************************************/
/*  subscripted_variable '[' subscript_list ']' */
// SYM_REF2(array_variable_c, subscripted_variable, subscript_list)
void *narrow_candidate_datatypes_c::visit(array_variable_c *symbol) {
	/* we need to check the data types of the expressions used for the subscripts... */
	symbol->subscript_list->accept(*this);
	return NULL;
}


/* subscript_list ',' subscript */
// SYM_LIST(subscript_list_c)
void *narrow_candidate_datatypes_c::visit(subscript_list_c *symbol) {
	for (int i = 0; i < symbol->n; i++) {
		for (unsigned int k = 0; k < symbol->elements[i]->candidate_datatypes.size(); k++) {
			if (is_ANY_INT_type(symbol->elements[i]->candidate_datatypes[k]))
				symbol->elements[i]->datatype = symbol->elements[i]->candidate_datatypes[k];
		}
		symbol->elements[i]->accept(*this);
	}
	return NULL;  
}



/************************************/
/* B 1.5 Program organization units */
/************************************/
/*********************/
/* B 1.5.1 Functions */
/*********************/
void *narrow_candidate_datatypes_c::visit(function_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations_list->accept(*this);
	if (debug) printf("Narrowing candidate data types list in body of function %s\n", ((token_c *)(symbol->derived_function_name))->value);
	symbol->function_body->accept(*this);
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}

/***************************/
/* B 1.5.2 Function blocks */
/***************************/
void *narrow_candidate_datatypes_c::visit(function_block_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations->accept(*this);
	if (debug) printf("Narrowing candidate data types list in body of FB %s\n", ((token_c *)(symbol->fblock_name))->value);
	symbol->fblock_body->accept(*this);
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}

/********************/
/* B 1.5.3 Programs */
/********************/
void *narrow_candidate_datatypes_c::visit(program_declaration_c *symbol) {
	search_varfb_instance_type = new search_varfb_instance_type_c(symbol);
	symbol->var_declarations->accept(*this);
	if (debug) printf("Narrowing candidate data types list in body of program %s\n", ((token_c *)(symbol->program_type_name))->value);
	symbol->function_block_body->accept(*this);
	delete search_varfb_instance_type;
	search_varfb_instance_type = NULL;
	return NULL;
}


/********************************/
/* B 1.7 Configuration elements */
/********************************/
void *narrow_candidate_datatypes_c::visit(configuration_declaration_c *symbol) {
	// TODO !!!
	/* for the moment we must return NULL so semantic analysis of remaining code is not interrupted! */
	return NULL;
}


/****************************************/
/* B.2 - Language IL (Instruction List) */
/****************************************/
/***********************************/
/* B 2.1 Instructions and Operands */
/***********************************/

/*| instruction_list il_instruction */
// SYM_LIST(instruction_list_c)
void *narrow_candidate_datatypes_c::visit(instruction_list_c *symbol) {
	/* In order to execute the narrow algoritm correctly, we need to go through the instructions backwards,
	 * so we can not use the base class' visitor 
	 */
	for(int i = symbol->n-1; i >= 0; i--) {
		symbol->elements[i]->accept(*this);
	}
	return NULL;
}

/* | label ':' [il_incomplete_instruction] eol_list */
// SYM_REF2(il_instruction_c, label, il_instruction)
// void *visit(instruction_list_c *symbol);
void *narrow_candidate_datatypes_c::visit(il_instruction_c *symbol) {
	if (NULL == symbol->il_instruction) {
		/* this empty/null il_instruction cannot generate the desired datatype. We pass on the request to the previous il instruction. */
		set_datatype_in_prev_il_instructions(symbol->datatype, symbol);
	} else {
		il_instruction_c tmp_prev_il_instruction(NULL, NULL);
		/* the narrow algorithm will need access to the intersected candidate_datatype lists of all prev_il_instructions, as well as the 
		 * list of the prev_il_instructions.
		 * Instead of creating two 'global' (within the class) variables, we create a single il_instruction_c variable (fake_prev_il_instruction),
		 * and shove that data into this single variable.
		 */
		tmp_prev_il_instruction.prev_il_instruction = symbol->prev_il_instruction;
		intersect_prev_candidate_datatype_lists(&tmp_prev_il_instruction);
		/* Tell the il_instruction the datatype that it must generate - this was chosen by the next il_instruction (remember: we are iterating backwards!) */
		fake_prev_il_instruction = &tmp_prev_il_instruction;
		symbol->il_instruction->datatype = symbol->datatype;
		symbol->il_instruction->accept(*this);
		fake_prev_il_instruction = NULL;
	}
	return NULL;
}




// void *visit(instruction_list_c *symbol);
void *narrow_candidate_datatypes_c::visit(il_simple_operation_c *symbol) {
	/* Tell the il_simple_operator the datatype that it must generate - this was chosen by the next il_instruction (we iterate backwards!) */
	symbol->il_simple_operator->datatype = symbol->datatype;
	/* recursive call to see whether data types are compatible */
	il_operand = symbol->il_operand;
	symbol->il_simple_operator->accept(*this);
	il_operand = NULL;
	return NULL;
}

/* | function_name [il_operand_list] */
/* NOTE: The parameters 'called_function_declaration' and 'extensible_param_count' are used to pass data between the stage 3 and stage 4. */
// SYM_REF2(il_function_call_c, function_name, il_operand_list, symbol_c *called_function_declaration; int extensible_param_count;)
void *narrow_candidate_datatypes_c::visit(il_function_call_c *symbol) {
	/* The first parameter of a non formal function call in IL will be the 'current value' (i.e. the prev_il_instruction)
	 * In order to be able to handle this without coding special cases, we will simply prepend that symbol
	 * to the il_operand_list, and remove it after calling handle_function_call().
	 * However, since handle_function_call() will be recursively calling all parameter, and we don't want
	 * to do that for the prev_il_instruction (since it has already been visited by the fill_candidate_datatypes_c)
	 * we create a new ____ symbol_c ____ object, and copy the relevant info to/from that object before/after
	 * the call to handle_function_call().
	 *
	 * However, if no further paramters are given, then il_operand_list will be NULL, and we will
	 * need to create a new object to hold the pointer to prev_il_instruction.
	 * This change will also be undone later in print_datatypes_error_c.
	 */
	symbol_c param_value = *fake_prev_il_instruction; /* copy the candidate_datatypes list */
	if (NULL == symbol->il_operand_list)  symbol->il_operand_list = new il_operand_list_c;
	if (NULL == symbol->il_operand_list)  ERROR;

	((list_c *)symbol->il_operand_list)->insert_element(&param_value, 0);

	generic_function_call_t fcall_param = {
		/* fcall_param.function_name               = */ symbol->function_name,
		/* fcall_param.nonformal_operand_list      = */ symbol->il_operand_list,
		/* fcall_param.formal_operand_list         = */ NULL,
		/* enum {POU_FB, POU_function} POU_type    = */ generic_function_call_t::POU_function,
		/* fcall_param.candidate_functions         = */ symbol->candidate_functions,
		/* fcall_param.called_function_declaration = */ symbol->called_function_declaration,
		/* fcall_param.extensible_param_count      = */ symbol->extensible_param_count
	};

	narrow_function_invocation(symbol, fcall_param);
	set_datatype_in_prev_il_instructions(param_value.datatype, fake_prev_il_instruction);

	/* Undo the changes to the abstract syntax tree we made above... */
	((list_c *)symbol->il_operand_list)->remove_element(0);
	if (((list_c *)symbol->il_operand_list)->n == 0) {
		/* if the list becomes empty, then that means that it did not exist before we made these changes, so we delete it! */
		delete 	symbol->il_operand_list;
		symbol->il_operand_list = NULL;
	}

	return NULL;
}


/* | il_expr_operator '(' [il_operand] eol_list [simple_instr_list] ')' */
// SYM_REF3(il_expression_c, il_expr_operator, il_operand, simple_instr_list);
void *narrow_candidate_datatypes_c::visit(il_expression_c *symbol) {
  /* first handle the operation (il_expr_operator) that will use the result coming from the parenthesised IL list (i.e. simple_instr_list) */
  symbol->il_expr_operator->datatype = symbol->datatype;
  il_operand = symbol->simple_instr_list; /* This is not a bug! The parenthesised expression will be used as the operator! */
  symbol->il_expr_operator->accept(*this);

  /* now give the parenthesised IL list a chance to narrow the datatypes */
  /* The datatype that is must return was set by the call symbol->il_expr_operator->accept(*this) */
  il_instruction_c *save_fake_prev_il_instruction = fake_prev_il_instruction; /*this is not really necessary, but lets play it safe */
  symbol->simple_instr_list->accept(*this);
  fake_prev_il_instruction = save_fake_prev_il_instruction;
  return NULL;
}


/*   il_call_operator prev_declared_fb_name
 * | il_call_operator prev_declared_fb_name '(' ')'
 * | il_call_operator prev_declared_fb_name '(' eol_list ')'
 * | il_call_operator prev_declared_fb_name '(' il_operand_list ')'
 * | il_call_operator prev_declared_fb_name '(' eol_list il_param_list ')'
 */
/* NOTE: The parameter 'called_fb_declaration'is used to pass data between stage 3 and stage4 (although currently it is not used in stage 4 */
// SYM_REF4(il_fb_call_c, il_call_operator, fb_name, il_operand_list, il_param_list, symbol_c *called_fb_declaration)
void *narrow_candidate_datatypes_c::visit(il_fb_call_c *symbol) {
	symbol_c *fb_decl = symbol->called_fb_declaration;
	
	/* Although a call to a non-declared FB is a semantic error, this is currently caught by stage 2! */
	if (NULL == fb_decl) ERROR;
	if (NULL != symbol->il_operand_list)  narrow_nonformal_call(symbol, fb_decl);
	if (NULL != symbol->  il_param_list)     narrow_formal_call(symbol, fb_decl);

	/* Let the il_call_operator (CAL, CALC, or CALCN) set the datatype of prev_il_instruction... */
	symbol->il_call_operator->datatype = symbol->datatype;
	symbol->il_call_operator->accept(*this);
	return NULL;
}


/* | function_name '(' eol_list [il_param_list] ')' */
/* NOTE: The parameter 'called_function_declaration' is used to pass data between the stage 3 and stage 4. */
// SYM_REF2(il_formal_funct_call_c, function_name, il_param_list, symbol_c *called_function_declaration; int extensible_param_count;)
void *narrow_candidate_datatypes_c::visit(il_formal_funct_call_c *symbol) {
	generic_function_call_t fcall_param = {
		/* fcall_param.function_name               = */ symbol->function_name,
		/* fcall_param.nonformal_operand_list      = */ NULL,
		/* fcall_param.formal_operand_list         = */ symbol->il_param_list,
		/* enum {POU_FB, POU_function} POU_type    = */ generic_function_call_t::POU_function,
		/* fcall_param.candidate_functions         = */ symbol->candidate_functions,
		/* fcall_param.called_function_declaration = */ symbol->called_function_declaration,
		/* fcall_param.extensible_param_count      = */ symbol->extensible_param_count
	};
  
	narrow_function_invocation(symbol, fcall_param);
	/* The desired datatype of the previous il instruction was already set by narrow_function_invocation() */
	return NULL;
}


//     void *visit(il_operand_list_c *symbol);


/* | simple_instr_list il_simple_instruction */
/* This object is referenced by il_expression_c objects */
void *narrow_candidate_datatypes_c::visit(simple_instr_list_c *symbol) {
	if (symbol->n > 0)
		symbol->elements[symbol->n - 1]->datatype = symbol->datatype;

	for(int i = symbol->n-1; i >= 0; i--) {
		symbol->elements[i]->accept(*this);
	}
	return NULL;
}


// SYM_REF1(il_simple_instruction_c, il_simple_instruction, symbol_c *prev_il_instruction;)
void *narrow_candidate_datatypes_c::visit(il_simple_instruction_c *symbol)	{
  if (symbol->prev_il_instruction.size() > 1) ERROR; /* There should be no labeled insructions inside an IL expression! */
    
  il_instruction_c tmp_prev_il_instruction(NULL, NULL);
  /* the narrow algorithm will need access to the intersected candidate_datatype lists of all prev_il_instructions, as well as the 
   * list of the prev_il_instructions.
   * Instead of creating two 'global' (within the class) variables, we create a single il_instruction_c variable (fake_prev_il_instruction),
   * and shove that data into this single variable.
   */
  if (symbol->prev_il_instruction.size() > 0)
    tmp_prev_il_instruction.candidate_datatypes = symbol->prev_il_instruction[0]->candidate_datatypes;
  tmp_prev_il_instruction.prev_il_instruction = symbol->prev_il_instruction;
  
   /* copy the candidate_datatypes list */
  fake_prev_il_instruction = &tmp_prev_il_instruction;
  symbol->il_simple_instruction->datatype = symbol->datatype;
  symbol->il_simple_instruction->accept(*this);
  fake_prev_il_instruction = NULL;
  return NULL;
}

//     void *visit(il_param_list_c *symbol);
//     void *visit(il_param_assignment_c *symbol);
//     void *visit(il_param_out_assignment_c *symbol);


/*******************/
/* B 2.2 Operators */
/*******************/

void *narrow_candidate_datatypes_c::handle_il_instruction(symbol_c *symbol) {
	if (NULL == symbol->datatype)
		/* next IL instructions were unable to determine the datatype this instruction should produce */
		return NULL;
	/* NOTE 1: the il_operand __may__ be pointing to a parenthesized list of IL instructions. 
	 * e.g.  LD 33
	 *       AND ( 45
	 *            OR 56
	 *            )
	 *       When we handle the first 'AND' IL_operator, the il_operand will point to an simple_instr_list_c.
	 *       In this case, when we call il_operand->accept(*this);, the prev_il_instruction pointer will be overwritten!
	 *
	 *       We must therefore set the prev_il_instruction->datatype = symbol->datatype;
	 *       __before__ calling il_operand->accept(*this) !!
	 *
	 * NOTE 2: We do not need to call prev_il_instruction->accept(*this), as the object to which prev_il_instruction
	 *         is pointing to will be later narrowed by the call from the for() loop of the instruction_list_c
	 *         (or simple_instr_list_c), which iterates backwards.
	 */
	/* set the desired datatype of the previous il instruction */
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	  
	/* set the datatype for the operand */
	il_operand->datatype = symbol->datatype;
	il_operand->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(LD_operator_c *symbol)   {
	if (NULL == symbol->datatype)
		/* next IL instructions were unable to determine the datatype this instruction should produce */
		return NULL;
	/* set the datatype for the operand */
	il_operand->datatype = symbol->datatype;
	il_operand->accept(*this);
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(LDN_operator_c *symbol)  {
	if (NULL == symbol->datatype)
		/* next IL instructions were unable to determine the datatype this instruction should produce */
		return NULL;
	/* set the datatype for the operand */
	il_operand->datatype = symbol->datatype;
	il_operand->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(ST_operator_c *symbol) {
	if (debug) printf("narrow_candidate_datatypes_c::visit(ST_operator_c *symbol) called.\n");
	if (symbol->candidate_datatypes.size() != 1)
		return NULL;
	symbol->datatype = symbol->candidate_datatypes[0];
	/* set the datatype for the operand */
	il_operand->datatype = symbol->datatype;
	il_operand->accept(*this);
	/* set the desired datatype of the previous il instruction */
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(STN_operator_c *symbol) {
	if (symbol->candidate_datatypes.size() != 1)
		return NULL;
	symbol->datatype = symbol->candidate_datatypes[0];
	/* set the datatype for the operand */
	il_operand->datatype = symbol->datatype;
	il_operand->accept(*this);
	/* set the desired datatype of the previous il instruction */
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(NOT_operator_c *symbol) {
  /* TODO: ... */
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(S_operator_c *symbol)  {
  /* TODO: what if this is a FB call? */
	return handle_il_instruction(symbol);
}
void *narrow_candidate_datatypes_c::visit(R_operator_c *symbol)  {
  /* TODO: what if this is a FB call? */
	return handle_il_instruction(symbol);
}


void *narrow_candidate_datatypes_c::visit(  S1_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "S1",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  R1_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "R1",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit( CLK_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "CLK", symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  CU_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "CU",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  CD_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "CD",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  PV_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "PV",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  IN_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "IN",  symbol->called_fb_declaration);}
void *narrow_candidate_datatypes_c::visit(  PT_operator_c *symbol)  {return narrow_implicit_il_fb_call(symbol, "PT",  symbol->called_fb_declaration);}


void *narrow_candidate_datatypes_c::visit( AND_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  OR_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( XOR_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(ANDN_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( ORN_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(XORN_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( ADD_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( SUB_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( MUL_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( DIV_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit( MOD_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  GT_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  GE_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  EQ_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  LT_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  LE_operator_c *symbol)  {return handle_il_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(  NE_operator_c *symbol)  {return handle_il_instruction(symbol);}


// SYM_REF0(CAL_operator_c)
/* called from il_fb_call_c (symbol->il_call_operator->accpet(*this) ) */
void *narrow_candidate_datatypes_c::visit(CAL_operator_c *symbol) {
	/* set the desired datatype of the previous il instruction */
	/* This FB call does not change the value in the current/default IL variable, so we pass the required datatype to the previous IL instruction */
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}


void *narrow_candidate_datatypes_c::narrow_conditional_flow_control_IL_instruction(symbol_c *symbol) {
	/* if the next IL instructions needs us to provide a datatype other than a bool, 
	 * then we have an internal compiler error - most likely in fill_candidate_datatypes_c 
	 */
	if ((NULL != symbol->datatype) && (!is_type(symbol->datatype, bool_type_name_c))) ERROR;
	if (symbol->candidate_datatypes.size() > 1) ERROR;

	/* NOTE: If there is no IL instruction following this CALC, CALCN, JMPC, JMPC, ..., instruction,
	 *       we must still provide a bool_type_name_c datatype (if possible, i.e. if it exists in the candidate datatype list).
	 *       If it is not possible, we set it to NULL
	 */
	if (symbol->candidate_datatypes.size() == 0)    symbol->datatype = NULL;
	else    symbol->datatype = symbol->candidate_datatypes[0]; /* i.e. a bool_type_name_c! */
	if ((NULL != symbol->datatype) && (!is_type(symbol->datatype, bool_type_name_c))) ERROR;

	/* set the required datatype of the previous IL instruction, i.e. a bool_type_name_c! */
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}


// SYM_REF0(CALC_operator_c)
// SYM_REF0(CALCN_operator_c)
/* called from visit(il_fb_call_c *) {symbol->il_call_operator->accpet(*this)} */
void *narrow_candidate_datatypes_c::visit( CALC_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(CALCN_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}


void *narrow_candidate_datatypes_c::visit(RET_operator_c *symbol) {
	/* set the desired datatype of the previous il instruction */
	/* This RET instruction does not change the value in the current/default IL variable, so we pass the required datatype to the previous IL instruction.
	 * Actually this should always be NULL, otherwise we have a bug in the flow_control_analysis_c
	 * However, since that class has not yet been completely finished, we do not yet check this assertion!
	 */
// 	if (NULL != symbol->datatype) ERROR;
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit( RETC_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(RETCN_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}

void *narrow_candidate_datatypes_c::visit(JMP_operator_c *symbol) {
	/* set the desired datatype of the previous il instruction */
	/* This JMP instruction does not change the value in the current/default IL variable, so we pass the required datatype to the previous IL instruction.
	 * Actually this should always be NULL, otherwise we have a bug in the flow_control_analysis_c
	 * However, since that class has not yet been completely finished, we do not yet check this assertion!
	 */
// 	if (NULL != symbol->datatype) ERROR;
	set_datatype_in_prev_il_instructions(symbol->datatype, fake_prev_il_instruction);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit( JMPC_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}
void *narrow_candidate_datatypes_c::visit(JMPCN_operator_c *symbol) {return narrow_conditional_flow_control_IL_instruction(symbol);}

/* Symbol class handled together with function call checks */
// void *visit(il_assign_operator_c *symbol, variable_name);
/* Symbol class handled together with function call checks */
// void *visit(il_assign_operator_c *symbol, option, variable_name);


/***************************************/
/* B.3 - Language ST (Structured Text) */
/***************************************/
/***********************/
/* B 3.1 - Expressions */
/***********************/

void *narrow_candidate_datatypes_c::visit(or_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(xor_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (is_type_equal(symbol->l_exp->candidate_datatypes[i], symbol->r_exp->candidate_datatypes[j])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(and_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(equ_expression_c *symbol) {
	/* Here symbol->datatype has already assigned to BOOL
	 * In conditional symbols like =, <>, =<, <, >, >= we have to set
	 * l_exp and r_exp expression matched with compatible type.
	 * Example:
	 * 		INT#14 = INT#81
	 * 		equ_expression_c symbol->datatype = BOOL from top visit
	 * 		symbol->l_exp->datatype => INT
	 * 		symbol->r_exp->datatype => INT
	 */
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])) {
				/*
				 * We do not need to check whether the type is an ANY_ELEMENTARY here.
				 * That was already done in fill_candidate_datatypes_c.
				 */
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->r_exp->datatype = selected_type;
	}
	else
		ERROR;

	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(notequ_expression_c *symbol)  {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(lt_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_type(symbol->l_exp->candidate_datatypes[i])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(gt_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_type(symbol->l_exp->candidate_datatypes[i])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(le_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_type(symbol->l_exp->candidate_datatypes[i])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(ge_expression_c *symbol) {
	symbol_c * selected_type = NULL;
	for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
		for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
			if (typeid(*symbol->l_exp->candidate_datatypes[i]) == typeid(*symbol->r_exp->candidate_datatypes[j])
					&& is_ANY_ELEMENTARY_type(symbol->l_exp->candidate_datatypes[i])) {
				selected_type = symbol->l_exp->candidate_datatypes[i];
				break;
			}
		}
	}

	if (NULL != selected_type) {
		symbol->l_exp->datatype = selected_type;
		symbol->l_exp->accept(*this);
		symbol->r_exp->datatype = selected_type;
		symbol->r_exp->accept(*this);
	}
	else
		ERROR;
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(add_expression_c *symbol) {
	int count = 0;

	if (is_ANY_NUM_compatible(symbol->datatype)) {
		symbol->l_exp->datatype = symbol->datatype;
		symbol->r_exp->datatype = symbol->datatype;
		count++;
	} else {
		/* TIME data type */
		for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
			for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
				/* test widening compatibility */
				if (is_widening_compatible(symbol->l_exp->candidate_datatypes[i],
						symbol->r_exp->candidate_datatypes[j],
						symbol->datatype, widen_ADD_table)) {
					symbol->l_exp->datatype = symbol->l_exp->candidate_datatypes[i];
					symbol->r_exp->datatype = symbol->r_exp->candidate_datatypes[j];
					count ++;
				}
			}
		}
	}
	if (count > 1)
		ERROR;
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	return NULL;
}



void *narrow_candidate_datatypes_c::visit(sub_expression_c *symbol) {
	int count = 0;

	if (is_ANY_NUM_compatible(symbol->datatype)) {
		symbol->l_exp->datatype = symbol->datatype;
		symbol->r_exp->datatype = symbol->datatype;
		count++;
	} else {
		/* TIME data type */
		for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
			for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
				/* test widening compatibility */
				if (is_widening_compatible(symbol->l_exp->candidate_datatypes[i],
						symbol->r_exp->candidate_datatypes[j],
						symbol->datatype, widen_SUB_table)) {
					symbol->l_exp->datatype = symbol->l_exp->candidate_datatypes[i];
					symbol->r_exp->datatype = symbol->r_exp->candidate_datatypes[j];
					count ++;
				}
			}
		}
	}
	if (count > 1)
		ERROR;
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(mul_expression_c *symbol) {
	int count = 0;

	if (is_ANY_NUM_compatible(symbol->datatype)) {
		symbol->l_exp->datatype = symbol->datatype;
		symbol->r_exp->datatype = symbol->datatype;
		count++;
	} else {
		/* TIME data type */
		for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
			for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
				/* test widening compatibility */
				if (is_widening_compatible(symbol->l_exp->candidate_datatypes[i],
						symbol->r_exp->candidate_datatypes[j],
						symbol->datatype, widen_MUL_table)) {
					symbol->l_exp->datatype = symbol->l_exp->candidate_datatypes[i];
					symbol->r_exp->datatype = symbol->r_exp->candidate_datatypes[j];
					count ++;
				}
			}
		}
	}
	if (count > 1)
		ERROR;
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(div_expression_c *symbol) {
	int count = 0;

	if (is_ANY_NUM_compatible(symbol->datatype)) {
		symbol->l_exp->datatype = symbol->datatype;
		symbol->r_exp->datatype = symbol->datatype;
		count++;
	} else {
		/* TIME data type */
		for(unsigned int i = 0; i < symbol->l_exp->candidate_datatypes.size(); i++) {
			for(unsigned int j = 0; j < symbol->r_exp->candidate_datatypes.size(); j++) {
				/* test widening compatibility */
				if (is_widening_compatible(symbol->l_exp->candidate_datatypes[i],
						symbol->r_exp->candidate_datatypes[j],
						symbol->datatype, widen_DIV_table)) {
					symbol->l_exp->datatype = symbol->l_exp->candidate_datatypes[i];
					symbol->r_exp->datatype = symbol->r_exp->candidate_datatypes[j];
					count ++;
				}
			}
		}
	}
	if (count > 1)
		ERROR;
	symbol->l_exp->accept(*this);
	symbol->r_exp->accept(*this);
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(mod_expression_c *symbol) {
	symbol->l_exp->datatype = symbol->datatype;
	symbol->l_exp->accept(*this);
	symbol->r_exp->datatype = symbol->datatype;
	symbol->r_exp->accept(*this);
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(power_expression_c *symbol) {
	symbol->l_exp->datatype = symbol->datatype;
	symbol->l_exp->accept(*this);
	if (! symbol->r_exp->candidate_datatypes.size()){
		symbol->r_exp->datatype = symbol->r_exp->candidate_datatypes[0];
		symbol->r_exp->accept(*this);
	}
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(neg_expression_c *symbol) {
	symbol->exp->datatype = symbol->datatype;
	symbol->exp->accept(*this);
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(not_expression_c *symbol) {
	symbol->exp->datatype = symbol->datatype;
	symbol->exp->accept(*this);
	return NULL;
}



/* NOTE: The parameter 'called_function_declaration', 'extensible_param_count' and 'candidate_functions' are used to pass data between the stage 3 and stage 4. */
/*    formal_param_list -> may be NULL ! */
/* nonformal_param_list -> may be NULL ! */
// SYM_REF3(function_invocation_c, function_name, formal_param_list, nonformal_param_list, symbol_c *called_function_declaration; int extensible_param_count; std::vector <symbol_c *> candidate_functions;)
void *narrow_candidate_datatypes_c::visit(function_invocation_c *symbol) {
	generic_function_call_t fcall_param = {
		/* fcall_param.function_name               = */ symbol->function_name,
		/* fcall_param.nonformal_operand_list      = */ symbol->nonformal_param_list,
		/* fcall_param.formal_operand_list         = */ symbol->formal_param_list,
		/* enum {POU_FB, POU_function} POU_type    = */ generic_function_call_t::POU_function,
		/* fcall_param.candidate_functions         = */ symbol->candidate_functions,
		/* fcall_param.called_function_declaration = */ symbol->called_function_declaration,
		/* fcall_param.extensible_param_count      = */ symbol->extensible_param_count
	};
  
	narrow_function_invocation(symbol, fcall_param);
	return NULL;
}

/********************/
/* B 3.2 Statements */
/********************/


/*********************************/
/* B 3.2.1 Assignment Statements */
/*********************************/

void *narrow_candidate_datatypes_c::visit(assignment_statement_c *symbol) {
	if (symbol->candidate_datatypes.size() != 1)
		return NULL;
	symbol->datatype = symbol->candidate_datatypes[0];
	symbol->l_exp->datatype = symbol->datatype;
	symbol->l_exp->accept(*this);
	symbol->r_exp->datatype = symbol->datatype;
	symbol->r_exp->accept(*this);
	return NULL;
}


/*****************************************/
/* B 3.2.2 Subprogram Control Statements */
/*****************************************/

void *narrow_candidate_datatypes_c::visit(fb_invocation_c *symbol) {
	/* Note: We do not use the symbol->called_fb_declaration value (set in fill_candidate_datatypes_c)
	 *       because we try to identify any other datatype errors in the expressions used in the 
	 *       parameters to the FB call (e.g.  fb_var(var1 * 56 + func(var * 43)) )
	 *       even it the call to the FB is invalid. 
	 *       This makes sense because it may be errors in those expressions which are
	 *       making this an invalid call, so it makes sense to point them out to the user!
	 */
	symbol_c *fb_decl = search_varfb_instance_type->get_basetype_decl(symbol->fb_name);

	/* Although a call to a non-declared FB is a semantic error, this is currently caught by stage 2! */
	if (NULL == fb_decl) ERROR;
	if (NULL != symbol->nonformal_param_list)  narrow_nonformal_call(symbol, fb_decl);
	if (NULL != symbol->   formal_param_list)     narrow_formal_call(symbol, fb_decl);

	return NULL;
}


/********************************/
/* B 3.2.3 Selection Statements */
/********************************/

void *narrow_candidate_datatypes_c::visit(if_statement_c *symbol) {
	for(unsigned int i = 0; i < symbol->expression->candidate_datatypes.size(); i++) {
		if (is_type(symbol->expression->candidate_datatypes[i], bool_type_name_c))
			symbol->expression->datatype = symbol->expression->candidate_datatypes[i];
	}
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	if (NULL != symbol->elseif_statement_list)
		symbol->elseif_statement_list->accept(*this);
	if (NULL != symbol->else_statement_list)
		symbol->else_statement_list->accept(*this);
	return NULL;
}


void *narrow_candidate_datatypes_c::visit(elseif_statement_c *symbol) {
	for (unsigned int i = 0; i < symbol->expression->candidate_datatypes.size(); i++) {
		if (is_type(symbol->expression->candidate_datatypes[i], bool_type_name_c))
			symbol->expression->datatype = symbol->expression->candidate_datatypes[i];
	}
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}

/* CASE expression OF case_element_list ELSE statement_list END_CASE */
// SYM_REF3(case_statement_c, expression, case_element_list, statement_list)
void *narrow_candidate_datatypes_c::visit(case_statement_c *symbol) {
	for (unsigned int i = 0; i < symbol->expression->candidate_datatypes.size(); i++) {
		if ((is_ANY_INT_type(symbol->expression->candidate_datatypes[i]))
				 || (search_base_type.type_is_enumerated(symbol->expression->candidate_datatypes[i])))
			symbol->expression->datatype = symbol->expression->candidate_datatypes[i];
	}
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	if (NULL != symbol->case_element_list) {
		symbol->case_element_list->datatype = symbol->expression->datatype;
		symbol->case_element_list->accept(*this);
	}
	return NULL;
}

/* helper symbol for case_statement */
// SYM_LIST(case_element_list_c)
void *narrow_candidate_datatypes_c::visit(case_element_list_c *symbol) {
	for (int i = 0; i < symbol->n; i++) {
		symbol->elements[i]->datatype = symbol->datatype;
		symbol->elements[i]->accept(*this);
	}
	return NULL;
}

/*  case_list ':' statement_list */
// SYM_REF2(case_element_c, case_list, statement_list)
void *narrow_candidate_datatypes_c::visit(case_element_c *symbol) {
	symbol->case_list->datatype = symbol->datatype;
	symbol->case_list->accept(*this);
	symbol->statement_list->accept(*this);
	return NULL;
}

// SYM_LIST(case_list_c)
void *narrow_candidate_datatypes_c::visit(case_list_c *symbol) {
	for (int i = 0; i < symbol->n; i++) {
		for (unsigned int k = 0; k < symbol->elements[i]->candidate_datatypes.size(); k++) {
			if (is_type_equal(symbol->datatype, symbol->elements[i]->candidate_datatypes[k]))
				symbol->elements[i]->datatype = symbol->elements[i]->candidate_datatypes[k];
		}
		/* NOTE: this may be an integer, a subrange_c, or a enumerated value! */
		symbol->elements[i]->accept(*this);
	}
	return NULL;
}


/********************************/
/* B 3.2.4 Iteration Statements */
/********************************/
void *narrow_candidate_datatypes_c::visit(for_statement_c *symbol) {
	/* Control variable */
	for(unsigned int i = 0; i < symbol->control_variable->candidate_datatypes.size(); i++) {
		if (is_ANY_INT_type(symbol->control_variable->candidate_datatypes[i])) {
			symbol->control_variable->datatype = symbol->control_variable->candidate_datatypes[i];
		}
	}
	symbol->control_variable->accept(*this);
	/* BEG expression */
	for(unsigned int i = 0; i < symbol->beg_expression->candidate_datatypes.size(); i++) {
		if (is_type_equal(symbol->control_variable->datatype,symbol->beg_expression->candidate_datatypes[i]) &&
				is_ANY_INT_type(symbol->beg_expression->candidate_datatypes[i])) {
			symbol->beg_expression->datatype = symbol->beg_expression->candidate_datatypes[i];
		}
	}
	symbol->beg_expression->accept(*this);
	/* END expression */
	for(unsigned int i = 0; i < symbol->end_expression->candidate_datatypes.size(); i++) {
		if (is_type_equal(symbol->control_variable->datatype,symbol->end_expression->candidate_datatypes[i]) &&
				is_ANY_INT_type(symbol->end_expression->candidate_datatypes[i])) {
			symbol->end_expression->datatype = symbol->end_expression->candidate_datatypes[i];
		}
	}
	symbol->end_expression->accept(*this);
	/* BY expression */
	if (NULL != symbol->by_expression) {
		for(unsigned int i = 0; i < symbol->by_expression->candidate_datatypes.size(); i++) {
			if (is_type_equal(symbol->control_variable->datatype,symbol->by_expression->candidate_datatypes[i]) &&
					is_ANY_INT_type(symbol->by_expression->candidate_datatypes[i])) {
				symbol->by_expression->datatype = symbol->by_expression->candidate_datatypes[i];
			}
		}
		symbol->by_expression->accept(*this);
	}
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(while_statement_c *symbol) {
	for (unsigned int i = 0; i < symbol->expression->candidate_datatypes.size(); i++) {
		if(is_BOOL_type(symbol->expression->candidate_datatypes[i]))
			symbol->expression->datatype = symbol->expression->candidate_datatypes[i];
	}
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}

void *narrow_candidate_datatypes_c::visit(repeat_statement_c *symbol) {
	for (unsigned int i = 0; i < symbol->expression->candidate_datatypes.size(); i++) {
		if(is_BOOL_type(symbol->expression->candidate_datatypes[i]))
			symbol->expression->datatype = symbol->expression->candidate_datatypes[i];
	}
	symbol->expression->accept(*this);
	if (NULL != symbol->statement_list)
		symbol->statement_list->accept(*this);
	return NULL;
}





