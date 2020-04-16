
class tWebobjects {  // The basic web objects classes
  public: // must have methods to support getting the data and posting the data, these are then overridden in child classes
  // by including primitives here, its easier to implement behaviours of forms involving multiple input types
  // functions include a series of validators to test if input is a number, time or date for example  
};

class tWebInput: public tWebObjects {
 private:
  tWebObjects vOwner; 
  // various web helper tools are implemented here to validate the input, the generic validator routine is overridden in children to effect the correct validator choice
  // in addition to the validators, there are several generic routines necessary for labeling the editors
  // there are also extended methods to facilitate JSON exchanges instead of HTTP
};
class tEdit: public tWebInput {
  // renders edit inputs, with or without masks
}
class tNumber: public tEdit {
  
}
class tTime: public tEdit {
  
}
class tPassword: public tWebInput {
  
}
class tPasswordConfirmation: public tWebInput {
  
}
class tWebForm: public tWebObjects {
  // webforms must maintain a list of webinput objects so that it can call them to render a web page, a json data frame and to process post data from both web and json POST requests
  // the webform is responsible for rendering the buttons on the form
};

class tWebPage: public tWebObjects {
  
}

class tConfigSys { 
 // this is the master class, it controls the entire config system including loading and saving of configuration data
 // its also responsible for rendering the page title and including persistent page data from local file or web source
 // a system should have at most one configSys file
};
