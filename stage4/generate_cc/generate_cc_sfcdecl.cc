/*
 * (c) 2007 Mario de Sousa, Laurent Bessard
 *
 * Offered to the public under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * This code is made available on the understanding that it will not be
 * used in safety-critical situations without a full and competent review.
 */

/*
 * An IEC 61131-3 IL and ST compiler.
 *
 * Based on the
 * FINAL DRAFT - IEC 61131-3, 2nd Ed. (2001-12-10)
 *
 */


/*
 * Conversion of sfc networks (i.e. SFC code).
 *
 * This is part of the 4th stage that generates
 * a c++ source program equivalent to the SFC, IL and ST
 * code.
 */




/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

class generate_cc_sfcdecl_c: protected generate_cc_typedecl_c {
  
  public:
      typedef enum {
        sfcdecl_sg,
        sfcinit_sg,
        stepdef_sg,
        stepundef_sg,
        actiondef_sg,
        actionundef_sg
       } sfcgeneration_t;
  
  private:
    char step_number;
    char action_number;
    char transition_number;
    
    sfcgeneration_t wanted_sfcgeneration;
    
  public:
    generate_cc_sfcdecl_c(stage4out_c *s4o_ptr, sfcgeneration_t sfcgeneration)
    : generate_cc_typedecl_c(s4o_ptr) {
      wanted_sfcgeneration = sfcgeneration;
    }
    ~generate_cc_sfcdecl_c(void) {}
    
    void print(symbol_c *symbol, const char *variable_prefix = NULL) {
      this->set_variable_prefix(variable_prefix);
      
      symbol->accept(*this);
    }
    
/*********************************************/
/* B.1.6  Sequential function chart elements */
/*********************************************/
    
    void *visit(sequential_function_chart_c *symbol) {
      step_number = 0;
      action_number = 0;
      transition_number = 0;
      switch (wanted_sfcgeneration) {
        case sfcdecl_sg:
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          
          /* steps table declaration */
          s4o.print(s4o.indent_spaces + "STEP step_list[");
          s4o.print_integer(step_number);
          s4o.print("];\n");
          s4o.print(s4o.indent_spaces + "UINT nb_steps = ");
          s4o.print_integer(step_number);
          s4o.print(";\n");
          
          /* actions table declaration */
          s4o.print(s4o.indent_spaces + "ACTION action_list[");
          s4o.print_integer(action_number);
          s4o.print("];\n");
          s4o.print(s4o.indent_spaces + "UINT nb_actions = ");
          s4o.print_integer(action_number);
          s4o.print(";\n");
          
          /* transitions table declaration */
          s4o.print(s4o.indent_spaces + "USINT transition_list[");
          s4o.print_integer(transition_number);
          s4o.print("];\n");
          break;
        case sfcinit_sg:
          /* steps table initialisation */
          s4o.print(s4o.indent_spaces + "STEP temp_step = {0, 0, 0};\n");
          s4o.print(s4o.indent_spaces + "for(UINT i = 0; i < ");
          print_variable_prefix();
          s4o.print("nb_steps; i++) {\n");
          s4o.indent_right();
          s4o.print(s4o.indent_spaces);
          print_variable_prefix();
          s4o.print("step_list[i] = temp_step;\n");
          s4o.indent_left();
          s4o.print(s4o.indent_spaces + "}\n");
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          
          /* actions table initialisation */
          s4o.print(s4o.indent_spaces + "ACTION temp_action = {0, 0, 0, 0, 0, 0};\n");
          s4o.print(s4o.indent_spaces + "for(UINT i = 0; i < ");
          print_variable_prefix();
          s4o.print("nb_actions; i++) {\n");
          s4o.indent_right();
          s4o.print(s4o.indent_spaces);
          print_variable_prefix();
          s4o.print("action_list[i] = temp_action;\n");
          s4o.indent_left();
          s4o.print(s4o.indent_spaces + "}\n");
          break;
        case stepdef_sg:
          s4o.print("// Steps definitions\n");
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          s4o.print("\n");
          break;
        case actiondef_sg:
          s4o.print("// Actions definitions\n");
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          s4o.print("\n");
          break;
        case stepundef_sg:
          s4o.print("// Steps undefinitions\n");
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          s4o.print("\n");
          break;
        case actionundef_sg:
          s4o.print("// Actions undefinitions\n");
          for(int i = 0; i < symbol->n; i++)
            symbol->elements[i]->accept(*this);
          s4o.print("\n");
          break;
      }
      return NULL;
    }
    
    void *visit(initial_step_c *symbol) {
      switch (wanted_sfcgeneration) {
        case sfcdecl_sg:
          step_number++;
          break;
        case sfcinit_sg:
          s4o.print(s4o.indent_spaces);
          print_variable_prefix();
          s4o.print("action_list[");
          s4o.print_integer(step_number);
          s4o.print("].state = 1;\n");
          step_number++;
          break;
        case stepdef_sg:
          s4o.print("#define ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->step_name->accept(*this);
          s4o.print(" ");
          s4o.print_integer(step_number);
          s4o.print("\n");
          step_number++;
          break;
        case stepundef_sg:
          s4o.print("#undef ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->step_name->accept(*this);
          s4o.print("\n");
          break;
        default:
          break;
      }
      return NULL;
    }
    
    void *visit(step_c *symbol) {
      switch (wanted_sfcgeneration) {
        case sfcdecl_sg:
          step_number++;
          break;
        case stepdef_sg:
          s4o.print("#define ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->step_name->accept(*this);
          s4o.print(" ");
          s4o.print_integer(step_number);
          s4o.print("\n");
          step_number++;
          break;
        case stepundef_sg:
          s4o.print("#undef ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->step_name->accept(*this);
          s4o.print("\n");
          break;
        default:
          break;
      }
      return NULL;
    }

    void *visit(transition_c *symbol) {
      switch (wanted_sfcgeneration) {
        case sfcdecl_sg:
          transition_number++;
          break;
        default:
          break;
      }
      return NULL;
    }

    void *visit(action_c *symbol) {
      switch (wanted_sfcgeneration) {
        case actiondef_sg:
          s4o.print("#define ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->action_name->accept(*this);
          s4o.print(" ");
          s4o.print_integer(action_number);
          s4o.print("\n");
          action_number++;
          break;
        case actionundef_sg:
          s4o.print("#undef ");
          s4o.print(SFC_STEP_ACTION_PREFIX);
          symbol->action_name->accept(*this);
          s4o.print("\n");
          break;
        case sfcdecl_sg:
          action_number++;
          break;
        default:
          break;
      }
      return NULL;
    }

}; /* generate_cc_sfcdecl_c */

