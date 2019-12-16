namespace UHDM {
  
  class base_class {
  public:
    base_class(){}
    virtual ~base_class(){}
  };

};


typedef struct uhdm_handle {
  uhdm_handle(unsigned int type, void* object) :
  m_type(type), m_object(object) {}
  unsigned int m_type;
  void* m_object;
} uhdm_handle;
