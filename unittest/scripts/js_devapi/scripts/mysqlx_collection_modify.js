// Assumptions: validate_crud_functions available
// Assumes __uripwd is defined as <user>:<pwd>@<host>:<plugin_port>
var mysqlx = require('mysqlx');

var mySession = mysqlx.getSession(__uripwd);

ensure_schema_does_not_exist(mySession, 'js_shell_test');

var schema = mySession.createSchema('js_shell_test');

// Creates a test collection and inserts data into it
var collection = schema.createCollection('collection1');

var result = collection.add({ name: 'jack', age: 17, gender: 'male' }).execute();
var result = collection.add({ name: 'adam', age: 15, gender: 'male' }).execute();
var result = collection.add({ name: 'brian', age: 14, gender: 'male' }).execute();
var result = collection.add({ name: 'alma', age: 13, gender: 'female' }).execute();
var result = collection.add({ name: 'carol', age: 14, gender: 'female' }).execute();
var result = collection.add({ name: 'donna', age: 16, gender: 'female' }).execute();
var result = collection.add({ name: 'angel', age: 14, gender: 'male' }).execute();

// ------------------------------------------------
// Collection.Modify Unit Testing: Dynamic Behavior
// ------------------------------------------------
//@ CollectionModify: valid operations after modify and set
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.set('name', 'dummy');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and unset empty
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.unset([]);
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);

//@ CollectionModify: valid operations after modify and unset list
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.unset(['name', 'type']);
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and unset multiple params
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.unset('name', 'type');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and merge
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.merge({ 'att': 'value', 'second': 'final' });
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and arrayInsert
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.arrayInsert('hobbies[3]', 'run');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and arrayAppend
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.arrayAppend('hobbies', 'skate');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after modify and arrayDelete
var crud = collection.modify('some_filter');
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete']);
var crud = crud.arrayDelete('hobbies[5]')
validate_crud_functions(crud, ['set', 'unset', 'merge', 'arrayInsert', 'arrayAppend', 'arrayDelete', 'sort', 'limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after sort
var crud = crud.sort(['name']);
validate_crud_functions(crud, ['limit', 'bind', 'execute']);

//@ CollectionModify: valid operations after limit
var crud = crud.limit(2);
validate_crud_functions(crud, ['bind', 'execute']);

//@ CollectionModify: valid operations after bind
var crud = collection.modify('name = :data').set('age', 15).bind('data', 'angel');
validate_crud_functions(crud, ['bind', 'execute']);

//@ CollectionModify: valid operations after execute
var result = crud.execute();
validate_crud_functions(crud, ['bind', 'execute']);

//@ Reusing CRUD with binding
print('Updated Angel:', result.affectedItemCount, '\n');
var result = crud.bind('data', 'carol').execute();
print('Updated Carol:', result.affectedItemCount, '\n');

// ----------------------------------------------
// Collection.Modify Unit Testing: Error Conditions
// ----------------------------------------------

//@# CollectionModify: Error conditions on modify
crud = collection.modify();
crud = collection.modify('   ');
crud = collection.modify(5);
crud = collection.modify('test = "2');

//@# CollectionModify: Error conditions on set
crud = collection.modify('some_filter').set();
crud = collection.modify('some_filter').set(45, 'whatever');
crud = collection.modify('some_filter').set('', 5);

//@# CollectionModify: Error conditions on unset
crud = collection.modify('some_filter').unset();
crud = collection.modify('some_filter').unset({});
crud = collection.modify('some_filter').unset({}, '');
crud = collection.modify('some_filter').unset(['name', 1]);
crud = collection.modify('some_filter').unset('');

//@# CollectionModify: Error conditions on merge
crud = collection.modify('some_filter').merge();
crud = collection.modify('some_filter').merge('');

//@# CollectionModify: Error conditions on arrayInsert
crud = collection.modify('some_filter').arrayInsert();
crud = collection.modify('some_filter').arrayInsert(5, 'another');
crud = collection.modify('some_filter').arrayInsert('', 'another');
crud = collection.modify('some_filter').arrayInsert('test', 'another');

//@# CollectionModify: Error conditions on arrayAppend
crud = collection.modify('some_filter').arrayAppend();
crud = collection.modify('some_filter').arrayAppend({}, '');
crud = collection.modify('some_filter').arrayAppend('', 45);
crud = collection.modify('some_filter').arrayAppend('data', mySession);

//@# CollectionModify: Error conditions on arrayDelete
crud = collection.modify('some_filter').arrayDelete();
crud = collection.modify('some_filter').arrayDelete(5);
crud = collection.modify('some_filter').arrayDelete('');
crud = collection.modify('some_filter').arrayDelete('test');

//@# CollectionModify: Error conditions on sort
crud = collection.modify('some_filter').unset('name').sort();
crud = collection.modify('some_filter').unset('name').sort(5);
crud = collection.modify('some_filter').unset('name').sort([]);
crud = collection.modify('some_filter').unset('name').sort(['name', 5]);
crud = collection.modify('some_filter').unset('name').sort('name', 5);

//@# CollectionModify: Error conditions on limit
crud = collection.modify('some_filter').unset('name').limit();
crud = collection.modify('some_filter').unset('name').limit('');

//@# CollectionModify: Error conditions on bind
crud = collection.modify('name = :data and age > :years').set('hobby', 'swim').bind();
crud = collection.modify('name = :data and age > :years').set('hobby', 'swim').bind(5, 5);
crud = collection.modify('name = :data and age > :years').set('hobby', 'swim').bind('another', 5)

//@# CollectionModify: Error conditions on execute
crud = collection.modify('name = :data and age > :years').set('hobby', 'swim').execute();
crud = collection.modify('name = :data and age > :years').set('hobby', 'swim').bind('years', 5).execute();

// ---------------------------------------
// Collection.Modify Unit Testing: Execution
// ---------------------------------------

//@# CollectionModify: Set Execution
var result = collection.modify('name = "brian"').set('alias', 'bri').set('last_name', 'black').set('age', mysqlx.expr('13+1')).execute();
print('Set Affected Rows:', result.affectedItemCount, '\n');

try {
  print("lastDocumentId:", result.lastDocumentId, "\n");
}
catch (err) {
  print("lastDocumentId:", err.message, "\n");
}

try {
  print("getLastDocumentId():", result.getLastDocumentId());
}
catch (err) {
  print("getLastDocumentId():", err.message, "\n");
}

try {
  print("lastDocumentIds:", result.lastDocumentIds);
}
catch (err) {
  print("lastDocumentIds:", err.message, "\n");
}

try {
  print("getLastDocumentIds():", result.getLastDocumentIds());
}
catch (err) {
  print("getLastDocumentIds():", err.message, "\n");
}

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print(dir(doc));

//@# CollectionModify: Set Execution Binding Array
var result = collection.modify('name = "brian"').set('hobbies', mysqlx.expr(':list')).bind('list', ['soccer', 'dance', 'read']).execute();
print('Set Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print(dir(doc));
print(doc.hobbies[0]);
print(doc.hobbies[1]);
print(doc.hobbies[2]);

//@ CollectionModify: Simple Unset Execution
var result = collection.modify('name = "brian"').unset('last_name').execute();
print('Unset Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print(dir(doc));

//@ CollectionModify: List Unset Execution
var result = collection.modify('name = "brian"').unset(['alias', 'age']).execute();
print('Unset Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print(dir(doc));

//@ CollectionModify: Merge Execution
var result = collection.modify('name = "brian"').merge({ last_name: 'black', age: 15, alias: 'bri', girlfriends: ['martha', 'karen'] }).execute();
print('Merge Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print("Brian's last_name:", doc.last_name, '\n');
print("Brian's age:", doc.age, '\n');
print("Brian's alias:", doc.alias, '\n');
print("Brian's first girlfriend:", doc.girlfriends[0], '\n');
print("Brian's second girlfriend:", doc.girlfriends[1], '\n');

//@ CollectionModify: arrayAppend Execution
var result = collection.modify('name = "brian"').arrayAppend('girlfriends', 'cloe').execute();
print('Array Append Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print("Brian's girlfriends:", doc.girlfriends.length);
print("Brian's last:", doc.girlfriends[2]);

//@ CollectionModify: arrayInsert Execution
var result = collection.modify('name = "brian"').arrayInsert('girlfriends[1]', 'samantha').execute();
print('Array Insert Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print("Brian's girlfriends:", doc.girlfriends.length, '\n');
print("Brian's second:", doc.girlfriends[1], '\n');

//@ CollectionModify: arrayDelete Execution
var result = collection.modify('name = "brian"').arrayDelete('girlfriends[2]').execute();
print('Array Delete Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('name = "brian"').execute();
var doc = result.fetchOne();
print("Brian's girlfriends:", doc.girlfriends.length, '\n');
print("Brian's third:", doc.girlfriends[2], '\n');

//@ CollectionModify: sorting and limit Execution
var result = collection.modify('age = 15').set('sample', 'in_limit').sort(['name']).limit(2).execute();
print('Affected Rows:', result.affectedItemCount, '\n');

var result = collection.find('age = 15').sort(['name']).execute();

//@ CollectionModify: sorting and limit Execution - 1
var doc = result.fetchOne();
print(dir(doc));

//@ CollectionModify: sorting and limit Execution - 2
var doc = result.fetchOne();
print(dir(doc));

//@ CollectionModify: sorting and limit Execution - 3
var doc = result.fetchOne();
print(dir(doc));

//@ CollectionModify: sorting and limit Execution - 4
var doc = result.fetchOne();
print(dir(doc));

//@<OUT> CollectionModify: help
collection.help('modify');

// Cleanup
mySession.dropSchema('js_shell_test');
mySession.close();
