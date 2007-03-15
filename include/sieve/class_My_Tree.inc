<?php



/* This class is inherited from the original 'Tree'
 *  class written by Heiko Hund.
 * It is partly rewritten to create a useable html interface 
 *  for each single sieve token. 
 * This gives us the ability to edit existing sieve filters. 
 */
class My_Tree extends Tree
{
  var $dumpFn_;
  var $dump_;

  var $mode_stack = array();
  var $pap		= array();

  var $add_new          = FALSE;
  var $add_new_id       = 0;
  var $add_type         = "top";
  var $add_element_type = "";
  
  var $Mode = "Structured";

  function execute()
  {
    foreach($_POST as $name => $value){
      if(preg_match("/Add_Test_Object_/",$name)) {
        $name = preg_replace("/Add_Test_Object_/","",$name);
        $name = preg_replace("/_(x|y)$/","",$name);
    
        $test_types_to_add = array(
          "address" =>_("Address"),
          "header"  =>_("Header"),
          "envelope"=>_("Envelope"),
          "size"    =>_("Size"),
          "exists"  =>_("Exists"),
          "allof"   =>_("All of"),
          "anyof"   =>_("Any of"),
          "true"    =>_("True"),
          "false"   =>_("False"));
 
        $smarty = get_smarty();
        $smarty->assign("ID",$name); 
        $smarty->assign("test_types_to_add",$test_types_to_add); 
        $ret = $smarty->fetch(get_template_path("templates/select_test_type.tpl",TRUE,dirname(__FILE__)));
        return($ret);
      }
    }

    return($this->dump());
  }

  /* Create a html interface for the current sieve filter 
   */
  function dump()
  {
    error_reporting(E_ALL);

    /**************
     * Handle new elements 
     **************/

    if(isset($_POST['select_new_element_type_cancel'])){
      $this->add_new = FALSE;
    }

    if($this->add_new){
  
      $element_types= array(
          "sieve_keep"      => _("Keep"),
          "sieve_comment"   => _("Comment"),
          "sieve_fileinto"  => _("File into"),
          "sieve_keep"      => _("Keep"),
          "sieve_discard"   => _("Discard"),
          "sieve_redirect"  => _("Redirect"),
          "sieve_reject"    => _("Reject"),
          "sieve_require"   => _("Require"),
          "sieve_stop"      => _("Stop"),
          "sieve_vacation"  => _("Vacation message"),
          "sieve_if"        => _("If"));


      /* Element selected */
      if(isset($_POST['element_type']) && isset($element_types[$_POST['element_type']])){
        $this->add_element_type = $_POST['element_type'];
      }

      /* Create new element and add it at the selected position */
      if(isset($_POST['select_new_element_type'])){

        $ele[] = new $this->add_element_type(NULL, preg_replace("/[^0-9]/","",microtime()));
        if($this->add_element_type == "sieve_if"){
          $ele[] = new sieve_block_start(NULL,preg_replace("/[^0-9]/","",microtime()));
          $ele[] = new sieve_block_end(NULL,preg_replace("/[^0-9]/","",microtime()));
        }
        $start = $end = array();
        $found = false;  

        /* Add above current element*/
        if($this->add_type == "top"){
          foreach($this->pap as $key => $obj){
            if($obj->object_id == $this->add_new_id){
              $found = true;
            }
            if(!$found){
              $start[] = $obj;
            }else{
              $end[] = $obj;
            }
          }    
        }else{
        /* Add below current element */
          foreach($this->pap as $key => $obj){
            if(!$found){
              $start[] = $obj;
            }else{
              $end[] = $obj;
            }
            if($obj->object_id == $this->add_new_id){
              $found = true;
            }
          }    
        }

        /* Only add, if current element could be located */
        if($found){
          $new = array();
          foreach($start as $obj){
            $new[] = $obj;
          }      
          foreach($ele as $el){
            $new[] = $el;
          }
          foreach($end as $obj){
            $new[] = $obj;
          }
          $this->pap = $new;
          $this->add_new = FALSE;
        }else{
          print_red(_("Something went wrong while adding a new entry."));
        }
      }

    }

    /* Only display select dialog if it is necessary */
    if($this->add_new){  
      $smarty = get_smarty();
      $smarty->assign("element_types",$element_types );
      $smarty->assign("element_type",$this->add_element_type);
      $str = $smarty->fetch(get_template_path("templates/add_element.tpl",TRUE,dirname(__FILE__)));
      return($str);
    }

    /* Only parse the tokens once */
    if(!count($this->pap)){
      $this->dump_ = "";
      $this->mode_stack = array();
      $this->pap = array();
      $this->doDump_(0, '', true);
    }

    /* Create html results */
    $smarty = get_smarty();

    $this -> dump_ = "";
    foreach($this->pap as $key => $object){
      if(is_object($object)){
        $this->dump_ .= preg_replace("/>/",">\n",$object->execute()); 
      }
    }
    
    return($this->dump_);
  }


  /* This function walks through the object tree generated by the "Parse" class.
   * All Commands will be resolved and grouped. So the Commands and their 
   *  parameter are combined. Like "IF" and ":comparator"...
   */  
  function doDump_($node_id, $prefix, $last,$num = 1)
  {
    /* Indicates that current comman will only be valid for a single line. 
     *  this command type will be removed from mode_stack after displaying it.
     */
    $rewoke_last = FALSE;

    /* Get node */ 
    $node = $this->nodes_[$node_id];

    /* This closes the last mode */
    if($node['class'] == "block-start"){
      $tmp = array_pop($this->mode_stack);
      $this->handle_elements($tmp,$node_id);
      $this->handle_elements(array("TYPE" => "block_start"),$node_id);
    }

    /* This closes the last mode */
    if($node['class'] == "block-end"){
      $tmp = array_pop($this->mode_stack);
      $this->handle_elements($tmp,$node_id);
      $this->handle_elements(array("TYPE" => "block_end"),$node_id);
    }

    /* Semicolon indicates a new command */
    if($node['class'] == "semicolon"){
      $tmp =array_pop($this->mode_stack);
      $this->handle_elements($tmp,$node_id);
    }

    /* Handle comments */
    if($node['class'] == "comment"){
      $this->mode_stack[] = array("TYPE" => $node['class']);
      $rewoke_last = TRUE;
    }

    /* Handle identifiers */
    $identifiers = array("else","if","elsif","end","reject","redirect","vacation","keep","discard","comment","fileinto","require","stop");
    if($node['class'] == "identifier" && in_array($node['text'],$identifiers)){
      $this->mode_stack[] = array("TYPE" => $node['text']); 
    }

    /* Add current node to current command stack */
    end($this->mode_stack);
    $key = key($this->mode_stack);
    $this->mode_stack[$key]['ELEMENTS'][] = $node;

    /* Remove last mode from mode stack, cause it was only valid for a single line */
    if($rewoke_last){
      $tmp =array_pop($this->mode_stack);
      $this->handle_elements($tmp,$node_id);
    }

    /* If this is a sub element, just call this for all childs */	
    if(isset($this->childs_[$node_id])){
      $childs = $this->childs_[$node_id];
      for ($i=0; $i<count($childs); ++$i)
      {
        $c_last = false;
        if ($i+1 == count($childs))
        {
          $c_last = true;
        }
        $this->doDump_($childs[$i], "", $num);
      }
    }
  }


  /* Create a class for each resolved object.
   * And append this class to a list of objects.
   */
  function handle_elements($data,$id)
  {
    if(!isset($data['TYPE'])){
      return;
    }
    $type = $data['TYPE'];
    
    $class_name= "sieve_".$type ;
    if(class_exists($class_name)){
      $this->pap[] = new $class_name($data,$id);
    }else{
      echo "<font color='red'>Missing : ".$class_name."</font>"."<br>";
    }
  }

  function save_object()
  {
    reset($this->pap);
    foreach($this->pap as $key => $obj){

      if(in_array(get_class($obj),array("sieve_if",
                                        "sieve_elsif",
                                        "sieve_vacation",
                                        "sieve_comment",
                                        "sieve_reject",
                                        "sieve_fileinto",
                                        "sieve_require",
                                        "sieve_redirect"))){


        if(isset($this->pap[$key]) && method_exists($this->pap[$key],"save_object")){
          $this->pap[$key]->save_object();
        }
      }

      $once = TRUE;
      foreach($_POST as $name => $value){

        if(isset($obj->object_id) && preg_match("/^Remove_Object_".$obj->object_id."_/",$name) && $once){
          $once = FALSE;
          $this->remove_object($key);
        }
        if(isset($obj->object_id) && preg_match("/^Move_Up_Object_".$obj->object_id."_/",$name) && $once){
          $this->move_up_down($key,"up");
          $once = FALSE;
        }
        if(isset($obj->object_id) && preg_match("/^Move_Down_Object_".$obj->object_id."_/",$name) && $once){
          $this->move_up_down($key,"down");
          $once = FALSE;
        }
        if(isset($obj->object_id) && preg_match("/^Add_Object_Top_".$obj->object_id."_/",$name) && $once){
          $this->add_new_object($obj->object_id,"top");
          $once = FALSE;
        }
        if(isset($obj->object_id) && preg_match("/^Add_Object_Bottom_".$obj->object_id."_/",$name) && $once){
          $this->add_new_object($obj->object_id,"bottom");
          $once = FALSE;
        }
      }
    }
  }


  /* Add a new object at the given position */
  function add_new_object($id,$top_bottom = "bottom")
  {
    $this->add_new    = TRUE;
    $this->add_new_id = $id;
    $this->add_type   = $top_bottom;
  }


  /* Remove the object at the given position */
  function remove_object($key_id)
  {
    $class = get_class($this->pap[$key_id]);
    if(in_array($class,array("sieve_if"))){
      $block_start= $key_id;
      $block_end  = $this->get_block_end($key_id);
      for($i = $block_start ; $i <= $block_end ; $i ++ ){
        unset($this->pap[$i]);
      }
    }else{
      unset($this->pap[$key_id]);
    }
    $tmp = array();
    foreach($this->pap as $element){
      $tmp[] = $element;
    }
    $this->pap = $tmp;
  }


  /* This function moves a given element to another position.
   * Single elements like "keep;" will simply be moved one posisition down/up.
   * Multiple elements like if-elsif-else will be moved as block. 
   * 
   *  $key_id     specified the element that should be moved.
   *  $direction  specifies to move elements "up" or "down"
   */
  function move_up_down($key_id,$direction = "down")
  {
     
    /* Get the current element to decide what to move. */ 
    $e_class = get_class($this->pap[$key_id]);
      
    if(in_array($e_class,array("sieve_if"))){
      $block_start= $key_id;
      $block_end  = $this->get_block_end($key_id);

      /* Depending on the direction move up down */
      if($direction == "down"){
        $next_free  = $this->_get_next_free_move_slot($block_end,$direction); 
      }else{
        $next_free  = $this->_get_next_free_move_slot($block_start,$direction); 
      }

      /* Move the given block */ 
      $this->move_multiple_elements($block_start,$block_end,$next_free);
    }

    if(in_array($e_class,array( "sieve_stop",
                                "sieve_keep",
                                "sieve_require",
                                "sieve_comment",
                                "sieve_stop",   
                                "sieve_reject", 
                                "sieve_fileinto",
                                "sieve_redirect", 
                                "sieve_discard"))){
      $this->move_single_element($key_id,$this->_get_next_free_move_slot($key_id,$direction));
    }
  }

  
  /* Move the given block to position */
  function move_multiple_elements($start,$end,$to)
  {
    /* Use class names for testing */
    $data = $this->pap;

    /* Get block to move */
    $block_to_move = array_slice($data,$start, ($end - $start +1));

    /* We want do move this block up */
    if($end > $to){
      
      /* Get start block */
      $start_block = array_slice($data,0,$to);

      /* Get Get all elements between the block to move 
       *  and next free position 
       */
      $block_to_free = array_slice($data,$to ,$start - $to );  
      $block_to_end = array_slice($data,$end+1);
      $new = array();
      foreach($start_block as $block){
        $new[] = $block;
      }
      foreach($block_to_move as $block){
        $new[] = $block;
      }
      foreach($block_to_free as $block){
        $new[] = $block;
      }
      foreach($block_to_end as $block){
        $new[] = $block;
      }
      $old = $this->pap;
      $this->pap = $new;
    }
    

    /* We want to move this block down. */
    if($to > $end){

      /* Get start block */
      $start_block = array_slice($data,0,$start);

      /* Get Get all elements between the block to move 
       *  and next free position 
       */
      $block_to_free = array_slice($data,$end +1,($to - $end  ));  

      /* Get the rest 
       */
      $block_to_end = array_slice($data,$to+1);

      $new = array();
      foreach($start_block as $block){
        $new[] = $block;
      }
      foreach($block_to_free as $block){
        $new[] = $block;
      }
      foreach($block_to_move as $block){
        $new[] = $block;
      }
      foreach($block_to_end as $block){
        $new[] = $block;
      }
      $old = $this->pap;
      $this->pap = $new;
    }
  }  

  
  /* This function returns the id of the element 
   *  where the current block ends  
   */
  function get_block_end($start)
  {
    /* Only execute if this is a really a block element. 
     * Block elements is only sieve_if
     */
    if(in_array(get_class($this->pap[$start]),array("sieve_if"))){

      $class      = get_class($this->pap[$start]);
      $next_class = get_class($this->pap[$start+1]);
      $block_depth = 0;

      $end = FALSE;

      while(!$end && $start < count($this->pap)){
 
        if($class == "sieve_block_start"){
          $block_depth ++;
        }

        if($class == "sieve_block_end"){
          $block_depth --;
        }

        if( $block_depth == 0 && 
            $class == "sieve_block_end" && 
            !in_array($next_class,array("sieve_else","sieve_elsif"))){
          $end = TRUE;
          $start --;
        }
        $start ++;       
        $class      = get_class($this->pap[$start]);
        $next_class = get_class($this->pap[$start+1]);
      }
    }
    return($start);
  }


  /* This function moves the single element at 
   *  position $from to position $to.
   */
  function move_single_element($from,$to)
  {
    if($from == $to) {
      return;
    }

    $ret = array();
    $tmp = $this->pap;

    $begin = array();
    $middle = array();
    $end = array();
    $element = $this->pap[$from];

    if($from > $to ){

      /* Get all element in fron to element to move */    
      if($from  != 0){
        $begin = array_slice($tmp,0,$to);
      }

      /* Get all elements between */
      $middle = array_slice($tmp,$to , ($from - ($to) ));  
    
      /* Get the rest */ 
      $end  = array_slice($tmp,$from+1);
 
      foreach($begin as $data){
        $ret[] = $data;
      }
      $ret[] = $element;
      foreach($middle as $data){
        $ret[] = $data;
      }
      foreach($end as $data){
        $ret[] = $data;
      }
      $this->pap = $ret;
    }
    if($from < $to ){

      /* Get all element in fron to element to move */    
      if($from  != 0){
        $begin = array_slice($tmp,0,$from);
      }

      /* Get all elements between */
      $middle = array_slice($tmp,$from+1 , ($to - ($from)));  
    
      /* Get the rest */ 
      $end  = array_slice($tmp,$to+1);
 
      foreach($begin as $data){
        $ret[] = $data;
      }
      foreach($middle as $data){
        $ret[] = $data;
      }
      $ret[] = $element;
      foreach($end as $data){
        $ret[] = $data;
      }
      $this->pap = $ret;
    }
  }


  /* Returns the next free position where we 
   *  can add a new sinle element 
   *    $key_id     = Current position
   *    $direction  = Forward or backward.
   */
  function _get_next_free_move_slot($key_id,$direction)
  {
    $last_class = "";
    $current_class ="";
    $next_class = "";

    /* After this elements we can add new elements 
     *  without having any trouble.
     */
    $allowed_to_add_after = array("sieve_keep",
                                  "sieve_require", 
                                  "sieve_stop", 
                                  "sieve_reject", 
                                  "sieve_fileinto", 
                                  "sieve_redirect", 
                                  "sieve_discard",
                                  "sieve_comment",
                                  "sieve_block_start"
                                 );

    /* Before this elements we can add new elements 
     *  without having any trouble.
     */
    $allowed_to_add_before = array("sieve_keep",
                                  "sieve_require", 
                                  "sieve_stop", 
                                  "sieve_reject", 
                                  "sieve_fileinto", 
                                  "sieve_comment",
                                  "sieve_redirect", 
                                  "sieve_discard",
                                  "sieve_if", 
                                  "sieve_block_end"
                                 );

    if($direction == "down"){
    
      $test = $this->pap;
      while($key_id < count($test)){
        if(($key_id+1) == count($test)) {
          return($key_id);
        }
        $key_id ++;
        $current_class  = get_class($test[$key_id]);
        if(in_array($current_class, $allowed_to_add_after)){
          return($key_id);
        } 
      } 
    }else{
  
      $test = $this->pap;
      if($key_id == 0) {
        return($key_id);
      }
      $key_id --;
      while($key_id >=0 ){
        $current_class  = get_class($test[$key_id]);
        if(in_array($current_class, $allowed_to_add_before)){
          return($key_id);
        } 
        $key_id --;
      }
      return(0);
    }
  }


  /* Need to be reviewed */
  function get_sieve_script()
  {
    $tmp ="";
    if(count($this->pap)){
      $buffer = "";    
      foreach($this->pap as $part)  {
        if(get_class($part) == "sieve_block_end"){
          $buffer = substr($buffer,0,strlen($buffer)-(strlen(SIEVE_INDENT_TAB)));
        }
        $tmp2 = $part->get_sieve_script_part();

        if(get_class($part) == "sieve_reject"){
          $tmp.=$tmp2;
        }else{

          $tmp3 = split("\n",$tmp2);
          foreach($tmp3 as $str){
            $str2 = trim($str);
            if(empty($str2)) continue;
            $tmp.= $buffer.$str."\n";
          }
        }
        if(get_class($part) == "sieve_block_start"){
          $buffer .= SIEVE_INDENT_TAB;
        }
      }
    }
    if(!preg_match("/Generated by GOsa - Gonicus System Administrator/",$tmp)){
      $tmp = "#Generated by GOsa - Gonicus System Administrator \n ".$tmp;
    }
    return($tmp);
  }

  function Add_Element()
  {
    $tmp = array("ELEMENTS" => array(array("class" => "qouted-string","text"=> "Bla bla, later more")));
    $this->pap[] = new sieve_comment($tmp,rand(1000,100000));
  }

  function check()
  {
		$msgs = array();

    /* Some logical checks. 
     *  like :  only sieve_comment can appear before require.
     */
    
    /* Ensure that there are no command before require 
     *  - Get id of last require tag
     *  - Collect object types in from of this tag. 
     *  - Check if there are tags collected that are not allowed 
     */
    $last_found_at = -1; 
    $objs = array();
    foreach($this->pap as $key => $obj){
      if(get_class($obj) == "sieve_require"){
        $last_found_at = $key;
      }
    }
    foreach($this->pap as $key => $obj){
      if($key == $last_found_at) break;  
      if(!in_array(get_class($obj),array("sieve_comment","sieve_require"))){
        $objs[] = get_class($obj);
      }
    }
    if(count($objs) && $last_found_at != -1){
      $str = _("Require must be the first command in the script.");  
      $msgs[] = $str;
      print_red($str);;
    }
    
		foreach($this->pap as $obj){
			$o_msgs = $obj->check();
			foreach($o_msgs as $o_msg){
				$msgs[] = $o_msg;
			}
		}
		return($msgs);
  }
}


/* Create valid sieve string/string-list 
 *  out of a given array
 */
function sieve_create_strings($data,$force_string = FALSE)
{
  $ret = "";
  if(is_array($data)){
    if(count($data) == 1){
      $ret = "\"";
      foreach($data as $dat){
        $ret .=$dat;
      }
      $ret.="\"";
    }else{
      foreach($data as $dat){
        $ret.= "\"";
        $ret.=$dat;
        $ret.="\", ";
      }
      $ret = preg_replace("/,$/","",trim($ret));
      $ret = "[".$ret."]";
    }
  }else{

    $Multiline = preg_match("/\n/",$data);
    $data = preg_replace("/\r/","",$data);;

    if($Multiline && !$force_string){
      $ret = "text: \r\n".$data."\r\n.\r\n";
    }else{
      $ret = "\"".$data."\"";
    }
  }
  $ret = preg_replace("/\"\"/","\"",$ret);
  $ret = preg_replace("/\n/","\r\n",$ret);
  
  return($ret);
}

/* This checks if there is a string at the current position 
 *  in the token array. 
 * If there is a string list at the current position,
 *  this function will return a complete list of all
 *  strings used in this list.
 * It also returns an offset of the last token position 
 */
function sieve_get_strings($data,$id)
{
  $ret = array();
  if($data[$id]['class'] == "left-bracket"){
    while($data[$id]['class']  != "right-bracket" && $id < count($data)){
      
      if($data[$id]['class'] == "quoted-string"){
        $ret[] = $data[$id]['text'];
      }
      $id ++;
    }
  }elseif($data[$id]['class'] == "quoted-string"){
    $ret[] = $data[$id]['text'];
  }elseif($data[$id]['class'] == "number"){
    $ret[] = $data[$id]['text'];
  }elseif($data[$id]['class'] == "multi-line"){
    $str = trim(preg_replace("/^text:/","",$data[$id]['text']));
    $str = trim(preg_replace("/\.$/","",$str));
    $ret[] = $str;
  }
  
  return(array("OFFSET" => $id, "STRINGS" => $ret));
}

// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler:
?>