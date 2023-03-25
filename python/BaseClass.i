class BaseClass {
  friend Serializer;
  virtual ~BaseClass() = default;
  Serializer* GetSerializer() const { return serializer_; }
  virtual UHDM_OBJECT_TYPE UhdmType() = 0;
  virtual const BaseClass* VpiParent() = 0;
  virtual bool VpiParent(BaseClass* data) = 0;
  virtual std::string_view VpiFile() const = 0;
  virtual bool VpiFile(std::string_view data) = 0;
  virtual int VpiLineNo(); 
  virtual bool VpiLineNo(int data);
  virtual short int VpiColumnNo();
  virtual bool VpiColumnNo(short int data);
  virtual int VpiEndLineNo();
  virtual bool VpiEndLineNo(int data);
  virtual short int VpiEndColumnNo();
  virtual bool VpiEndColumnNo(short int data);
  virtual std::string_view VpiName();
  virtual std::string_view VpiDefName();
  virtual unsigned int VpiType() = 0;
  ClientData* Data();
  void Data(ClientData* data);
  virtual unsigned int UhdmId();
  virtual bool UhdmId(unsigned int id) = 0;
  virtual const BaseClass* GetByVpiName(std::string_view name);
  virtual std::tuple<const BaseClass*, UHDM_OBJECT_TYPE,
                     const std::vector<const BaseClass*>*>
  GetByVpiType(int type);
  typedef std::variant<int64_t, const char*> vpi_property_value_t;
  virtual vpi_property_value_t GetVpiPropertyValue(int property);
  virtual BaseClass* DeepClone(Serializer* serializer,
                               ElaboratorListener* elaborator,
                               BaseClass* parent) = 0;
  virtual int Compare(const BaseClass* const other, AnySet& visited);
  int Compare(const BaseClass* const other);
};
